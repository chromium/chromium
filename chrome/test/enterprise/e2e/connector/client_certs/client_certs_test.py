# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import os
import ntpath
import posixpath
from urllib.parse import urlparse

from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from cryptography import x509
from cryptography.hazmat.primitives import hashes

from infra import ChromeEnterpriseTestCase


@category('chrome_only')
@environment(file='../connector_test.asset.textpb')
class ClientCertsTest(ChromeEnterpriseTestCase):

  @before_all
  def setup(self):
    self.EnsureDirectory(self.win_config['dc'], r'c:\temp')
    self.EnsurePythonInstalled(self.win_config['dc'])
    self.InstallPipPackagesLatest(self.win_config['dc'], ['absl-py'])
    self.disable_domain_firewall(self.win_config['dc'])

    self.InstallChrome(self.win_config['client'])
    self.EnableUITest(self.win_config['client'])
    server_ca_cert_path = self.download_file_into_vm(
        self.win_config['client'],
        f'gs://{self.gsbucket}/secrets/certs/server-ca.pem')
    self.install_root_cert(self.win_config['client'], server_ca_cert_path)

  @test
  def test_client_cert_installed_and_used(self):
    # Admin console setup:
    # * Choose an owned test account `accountX@chromepizzatest.com` from the
    #   preprovisioned pool of users.
    # * Enable Google CA connector for OU.
    # * Copy `GoogleCertificateAuthority.pem` from the admin console to the
    #   GCS secrets.
    # * Override `AutoSelectCertificateForUrls` policy for OU with
    #   `{"pattern":"https://test1.com:443", "filter": {}}`. This adds test
    #   coverage for that policy, and is also more automation-friendly by not
    #   prompting the client to select a certificate to send to
    #   `test1.com:443`.
    server_script = self.path_from_test_dir(os.pardir, 'common',
                                            'echo_server.py')
    server_cert_path = self.download_file_into_vm(
        self.win_config['dc'], f'gs://{self.gsbucket}/secrets/certs/server.pem')
    server_key_path = self.download_file_into_vm(
        self.win_config['dc'], f'gs://{self.gsbucket}/secrets/certs/server.key')
    google_ca_cert_path = self.download_file_into_vm(
        self.win_config['dc'],
        f'gs://{self.gsbucket}/secrets/certs/GoogleCertificateAuthority.pem')

    test_script = self.path_from_test_dir('client_certs_webdriver_test.py')
    password = self.GetFileFromGCSBucket('secrets/account0-password')

    with self.RunScriptInBackground(self.win_config['dc'], server_script, [
        f'--cert={server_cert_path}',
        f'--key={server_key_path}',
        f'--verify_cert={google_ca_cert_path}',
    ]):
      self.RunUITest(
          self.win_config['client'],
          test_script,
          args=[
              '--account=account0@chromepizzatest.com',
              f'--password={password}',
          ],
          timeout=(15 * 60))
    raw_results = self.RunCommand(self.win_config['client'],
                                  r'Get-Content c:\temp\results.json')
    results = json.loads(raw_results)

    policies_by_name = results['policies']
    self.assertEqual({
        'value': '1',
        'scope': 'Current user',
        'source': 'Cloud',
    }, policies_by_name['ProvisionManagedClientCertificateForUser'])
    auto_select_cert_policy = policies_by_name['AutoSelectCertificateForUrls']
    patterns = json.loads(auto_select_cert_policy['value'])
    self.assertEqual(len(patterns), 1)
    self.assertEqual(
        json.loads(patterns[0]), {
            'pattern': 'https://test1.com:443',
            'filter': {},
        })
    self.assertEqual(auto_select_cert_policy['scope'], 'Current user')
    self.assertEqual(auto_select_cert_policy['source'], 'Cloud')

    fingerprints = results['fingerprints']
    self.assertEqual(fingerprints['connectors'], fingerprints['cert-manager'])
    raw_client_cert = base64.b64decode(fingerprints['server'])
    self.assertEqual(fingerprints['connectors'],
                     self.get_fingerprint_from_der(raw_client_cert))

  def disable_domain_firewall(self, instance_name: str):
    """Allow traffic to `instance_name` from other VMs in the AD domain."""
    cmd = 'Set-NetFirewallProfile -Profile Domain -Enabled False'
    self.clients[instance_name].RunPowershell(cmd)

  def install_root_cert(self, instance_name: str, cert_path: str):
    """Install a root certificate in a Windows VM's platform-specific store.

    Chrome automatically imports certificates from that store. The `cert_path`
    should be a path on the VM filesystem.
    """
    cmd = (f'Import-Certificate -FilePath {cert_path} '
           r'-CertStoreLocation cert:\\LocalMachine\Root')
    self.clients[instance_name].RunPowershell(cmd)

  def download_file_into_vm(self, instance_name: str, gcs_url: str) -> str:
    """Download a file from GCS into a VM and return the filesystem path.

    Note that this is different from `DownloadFile()`, which copies a file from
    a VM to this machine orchestrating the test.
    """
    basename = posixpath.basename(urlparse(gcs_url).path)
    dest = ntpath.join(r'c:\temp', basename)
    self.RunCommand(instance_name, f'gsutil cp {gcs_url} {dest}')
    return dest

  def get_fingerprint_from_der(self, der: bytes) -> str:
    cert = x509.load_der_x509_certificate(der)
    return cert.fingerprint(hashes.SHA256()).hex()

  def path_from_test_dir(self, *parts: str) -> str:
    return os.path.abspath(os.path.join(os.path.dirname(__file__), *parts))

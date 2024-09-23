# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from posixpath import join
import random
import string
import subprocess
import time

from absl import flags
from chrome_ent_test.infra.core import EnterpriseTestCase

FLAGS = flags.FLAGS
flags.DEFINE_string('chrome_installer', None,
                    'The path to the chrome installer')
flags.mark_flag_as_required('chrome_installer')

flags.DEFINE_string(
    'chromedriver', None,
    'The path to the chromedriver executable. If not specified, '
    'a chocholatey chromedriver packae will be installed and used.')

flags.DEFINE_string('omaha_installer', None,
                    'The path to the omaha 4 UpdaterSetup.exe')

flags.DEFINE_string('omaha_updater', None,
                    'The path to the omaha 4 Updater.exe')


class ChromeEnterpriseTestCase(EnterpriseTestCase):
  """Base class for Chrome enterprise test cases."""
  # dc is the domain controller host
  win_2012_config = {
      'client': 'client2012',
      'dc': 'win2012-dc',
  }

  win_2016_config = {
      'client': 'client2016',
      'dc': 'win2016-dc',
  }

  win_2019_config = {
      'client': 'client2019',
      'dc': 'win2019-dc',
  }

  win_2022_config = {
      'client': 'client2022',
      'dc': 'win2022-dc',
  }

  # Current Win Server version for testing
  win_config = win_2022_config

  def AddFirewallExclusion(self, instance_name):
    """Add-MpPreference to exclude some folders from defenser scan."""
    program_file = '"$Env:ProgramFiles"'
    program_file_x86 = '"$Env:ProgramFiles(x86)"'
    local_appdata = '"$Env:LOCALAPPDATA"'
    celab_path = r'"c:\cel\supporting_files"'
    updater_path = r'"c:\temp"'
    cmd = (r'Add-MpPreference -ExclusionPath ' + ', '.join([
        program_file, program_file_x86, local_appdata, celab_path, updater_path
    ]))
    self.clients[instance_name].RunPowershell(cmd)

  def InstallGoogleUpdater(self, instance_name):
    """Install Omaha4 client on VM."""
    if not FLAGS.omaha_installer:
      # No omaha installer/updater. Do nothing.
      logging.debug('--omaha_installer flag is empty.'
                    'Skip installing google updater.')
      return
    cmd = r'New-Item -ItemType Directory -Force -Path c:\temp'
    self.clients[instance_name].RunPowershell(cmd)
    installer = self.UploadFile(instance_name, FLAGS.omaha_installer,
                                r'c:\temp')
    cmd = installer + r' --install --system'
    self.RunCommand(instance_name, cmd)

  def WakeGoogleUpdater(self, instance_name):
    """Runs updater.exe to wake up Omaha 4 service."""
    if not FLAGS.omaha_updater:
      logging.debug('--omaha_updater flag is empty.' 'Skip run google updater.')
      return

    updater = self.UploadFile(instance_name, FLAGS.omaha_updater, r'c:\temp')
    cmd = (
        updater + r' --wake' + r' --enable-logging' +
        r' --vmodule=*/components/winhttp/*=2,*/components/update_client/*=2,'
        r'*/chrome/updater/*=2')
    self.RunCommand(instance_name, cmd)

  def GetChromeVersion(self, instance_name):
    """Get Chrome Version by querying Windows registry"""
    cmd = (
        r'reg query' +
        r' "HKLM\SOFTWARE\Google\Update\Clients\{8A69D345-D564-463C-AFF1-A69D9E530F96}"'
        + r' /reg:32 /v pv')
    chrome_version = self.RunCommand(instance_name, cmd)

    return chrome_version.decode().split()[-1]

  def RunGoogleUpdaterTaskSchedulerCommand(self, instance_name, cmd):
    """Run task scheduler powershell command to Google Updater"""
    script = r'Get-ScheduledTask -TaskPath \GoogleSystem\GoogleUpdater\ | ' + cmd
    return self.clients[instance_name].RunPowershell(script).decode().strip()

  def WaitForUpdateCheck(self, instance_name):
    """Wait for the updater task to be ready again"""
    max_wait_time_secs = 120
    total_wait_time_secs = 0
    delta_secs = 5

    while total_wait_time_secs < max_wait_time_secs:
      time.sleep(delta_secs)
      total_wait_time_secs += delta_secs
      state = self.RunGoogleUpdaterTaskSchedulerCommand(
          instance_name, 'Select -ExpandProperty "State"')
      if state == 'Ready':
        break

  def InstallChrome(self, instance_name, system_level=False):
    """Installs chrome.

    Currently supports two types of installer:
    - mini_installer.exe, and
    - *.msi

    system_level is False by default for all reporting connector
    e2e tests as it does not need Omaha client to be installed.
    For DTC e2e test, however,
    system_level should be set True so that it will be installed together
    with Omaha 4 client and then call powershell script to add
    chrome path to $Env:Path.

    Args:
      instance_name: the gcp instance.
      system_level: whether the chrome install with --system-level
        or not. By default, the value is False.
    """
    cmd = r'New-Item -ItemType Directory -Force -Path c:\temp'
    self.clients[instance_name].RunPowershell(cmd)
    file_name = self.UploadFile(instance_name, FLAGS.chrome_installer,
                                r'c:\temp')

    if file_name.lower().endswith('mini_installer.exe'):
      dir = os.path.dirname(os.path.abspath(__file__))
      self.UploadFile(instance_name, os.path.join(dir, 'installer_data'),
                      r'c:\temp')
      if system_level:
        cmd = (
            file_name + r' --installerdata=c:\temp\installer_data' +
            r' --system-level')
      else:
        cmd = file_name + r' --installerdata=c:\temp\installer_data'
    else:
      cmd = 'msiexec /i %s' % file_name

    self.RunCommand(instance_name, cmd)

    cmd = (
        r'powershell -File c:\cel\supporting_files\ensure_chromium_api_keys.ps1'
        r' -Path gs://%s/api/key') % self.gsbucket
    self.RunCommand(instance_name, cmd)
    if system_level:
      cmd = r'powershell -File c:\cel\supporting_files\add_chrome_path.ps1'
      self.RunCommand(instance_name, cmd)

  def SetPolicy(self, instance_name, policy_name, policy_value, policy_type):
    r"""Sets a Google Chrome policy in registry.

    Args:
      policy_name: the policy name.
        The name can contain \. In this case, the last segment will be the
        real policy name, while anything before will be part of the key.
    """
    segments = policy_name.split('\\')
    policy_name = segments[-1]

    # The policy will be set for both Chrome and Chromium, since only
    # googlers can build Chrome-branded executable.
    keys = [
        r'HKLM\Software\Policies\Google\Chrome',
        r'HKLM\Software\Policies\Chromium'
    ]
    for key in keys:
      if len(segments) >= 2:
        key += '\\' + '\\'.join(segments[:-1])

      cmd = (r"Set-GPRegistryValue -Name 'Default Domain Policy' "
             "-Key %s -ValueName %s -Value %s -Type %s") % (
                 key, policy_name, policy_value, policy_type)
      self.clients[instance_name].RunPowershell(cmd)

  def SetOmahaPolicy(self, instance_name, policy_name, policy_value,
                     policy_type):
    key = r'HKLM\Software\Policies\Google\Update'
    cmd = (r"Set-GPRegistryValue -Name 'Default Domain Policy' "
           "-Key %s -ValueName %s -Value %s -Type %s") % (
               key, policy_name, policy_value, policy_type)
    self.clients[instance_name].RunPowershell(cmd)

  def RemoveDeviceTrustKey(self, instance_name):
    """Removes a device trust key in registry.

    Args:
      instance_name: the name of the GCP VM machine.
    """
    cmd = (r'Remove-Item -Path HKLM:\SOFTWARE\Google\Chrome\DeviceTrust '
           '-Force -Verbose')
    self.clients[instance_name].RunPowershell(cmd)

  def RemovePolicy(self, instance_name, policy_name):
    """Removes a Google Chrome policy in registry.

    Args:
      instance_name: the name of the GCP VM machine.
      policy_name: the policy name.
    """
    segments = policy_name.split('\\')
    policy_name = segments[-1]

    keys = [
        r'HKLM\Software\Policies\Google\Chrome',
        r'HKLM\Software\Policies\Chromium'
    ]
    for key in keys:
      if len(segments) >= 2:
        key += '\\' + '\\'.join(segments[:-1])

      cmd = (r"Remove-GPRegistryValue -Name 'Default Domain Policy' "
             "-Key %s -ValueName %s") % (key, policy_name)
      self.clients[instance_name].RunPowershell(cmd)

  def GetFileFromGCSBucket(self, path):
    """Get file from GCS bucket"""
    path = "gs://%s/%s" % (self.gsbucket, path)
    cmd = r'gsutil cat ' + path
    return self.RunCommand(self.win_config['client'], cmd).rstrip().decode()

  def InstallWebDriver(self, instance_name):
    self.RunCommand(instance_name, r'md -Force c:\temp')
    self.EnsurePythonInstalled(instance_name)
    self.InstallPipPackagesLatest(instance_name,
                                  ['selenium', 'absl-py', 'pywin32', 'attrs'])

    temp_dir = 'C:\\temp\\'
    if FLAGS.chromedriver is None:
      # chromedriver flag is not specified. Install the chocolatey package.
      self.InstallChocolateyPackage(instance_name, 'chromedriver',
                                    '74.0.3729.60')
      self.RunCommand(
          instance_name, "copy %s %s" %
          (r"C:\ProgramData\chocolatey\lib\chromedriver\tools\chromedriver.exe",
           temp_dir))
    else:
      self.UploadFile(instance_name, FLAGS.chromedriver, temp_dir)

    dir = os.path.dirname(os.path.abspath(__file__))
    self.UploadFile(instance_name, os.path.join(dir, 'test_util.py'), temp_dir)

  def RunWebDriverTest(self, instance_name, test_file, args=[]):
    """Runs a python webdriver test on an instance.

    Args:
      instance_name: name of the instance.
      test_file: the path of the webdriver test file.
      args: the list of arguments passed to the test.

    Returns:
      the output."""
    # upload the test
    file_name = self.UploadFile(instance_name, test_file, r'c:\temp')

    # run the test
    args = subprocess.list2cmdline(args)
    self._pythonExecutablePath[instance_name] = (
        r'C:\ProgramData\chocolatey\lib\python\tools\python.exe')
    cmd = r'%s -u %s %s' % (self._pythonExecutablePath[instance_name],
                            file_name, args)
    return self.RunCommand(instance_name, cmd).decode()

  def EnableHistogramSupport(self, instance_name, base_path):
    """Enable histogram package support on an instance.

    Note that base_path is the path to chrome/test/enterprise/e2e/connector.

    Args:
      instance_name: name of the instance.
      base_path: the base path of the test in the chromium_src.
    """
    dest_path = join('c:', 'temp', 'histogram')
    cmd = r'New-Item -ItemType Directory -Force -Path ' + dest_path
    self.clients[instance_name].RunPowershell(cmd)

    self.UploadFile(
        self.win_config['client'],
        os.path.join(base_path, 'common', 'histogram', '__init__.py'),
        dest_path)
    self.UploadFile(self.win_config['client'],
                    os.path.join(base_path, 'common', 'histogram', 'util.py'),
                    dest_path)

  def EnableDemoAgent(self, instance_name):
    # enterprise/e2e/connector/common/demo_agent
    base_path = dir = os.path.dirname(
        os.path.dirname(os.path.abspath(__file__)))
    agent_path = os.path.join(base_path, 'connector', 'common', 'demo_agent')

    # create dest path
    dest_path = join('c:', 'temp', 'demo_agent')
    cmd = r'New-Item -ItemType Directory -Force -Path ' + dest_path
    self.clients[instance_name].RunPowershell(cmd)

    # Install Visual C++ Redistributable package as demo agent's dependency
    gspath = "gs://%s/%s" % (self.gsbucket, 'secrets/VC_redist.x64.exe')
    cmd = r'gsutil cp ' + gspath + ' ' + dest_path

    self.RunCommand(instance_name, cmd)

    cmd = r'C:\temp\demo_agent\VC_redist.x64.exe /passive'
    self.RunCommand(instance_name, cmd)

    # upload demo agent
    self.UploadFile(self.win_config['client'],
                    os.path.join(agent_path, 'agent.zip'), dest_path)
    cmd = r'Expand-Archive -Path c:\temp\demo_agent\agent.zip -DestinationPath c:\temp\demo_agent'
    self.clients[instance_name].RunPowershell(cmd)

  def RunUITest(self, instance_name, test_file, timeout=300, args=[]):
    """Runs a UI test on an instance.

    Args:
      instance_name: name of the instance.
      test_file: the path of the UI test file.
      timeout: the timeout in seconds. Default is 300,
               i.e. 5 minutes.
      args: the list of arguments passed to the test.

    Returns:
      the output."""
    # upload the test
    file_name = self.UploadFile(instance_name, test_file, r'c:\temp')

    # check for cel_ui_agent.exe running
    self._checkUIAgentRunningOnInstance(instance_name)

    # run the test.
    # note that '-u' flag is passed to enable unbuffered stdout and stderr.
    # Without this flag, if the test is killed because of timeout, we will not
    # get any output from stdout because the output is buffered. When this
    # happens it makes debugging really hard.
    args = subprocess.list2cmdline(args)
    self._pythonExecutablePath[instance_name] = (
        r'C:\ProgramData\chocolatey\lib\python\tools\python.exe')
    ui_test_cmd = r'%s -u %s %s' % (self._pythonExecutablePath[instance_name],
                                    file_name, args)
    cmd = (r'%s c:\cel\supporting_files\run_ui_test.py --timeout %s -- %s') % (
        self._pythonExecutablePath[instance_name], timeout, ui_test_cmd)
    return self.RunCommand(instance_name, cmd, timeout=timeout).decode()

  def _generatePassword(self):
    """Generates a random password."""
    s = [random.choice(string.ascii_lowercase) for _ in range(4)]
    s += [random.choice(string.ascii_uppercase) for _ in range(4)]
    s += [random.choice(string.digits) for _ in range(4)]
    random.shuffle(s)
    return ''.join(s)

  def _checkUIAgentRunningOnInstance(self, instance_name):
    self.RunCommand(instance_name, 'tasklist | findstr cel_ui_agent.exe')

  def _rebootInstance(self, instance_name):
    self.RunCommand(instance_name, 'shutdown /r /t 0')

    # wait a while for the instance to boot up
    time.sleep(2 * 60)

  def EnableUITest(self, instance_name):
    """Configures the instance so that UI tests can be run on it."""
    self.InstallWebDriver(instance_name)
    self.InstallChocolateyPackage(instance_name, 'chocolatey_core_extension',
                                  '1.3.3')
    self.InstallChocolateyPackageLatest(instance_name, 'sysinternals')
    self.InstallPipPackagesLatest(instance_name,
                                  ['pywinauto', 'pyperclip', 'requests'])

    ui_test_user = 'ui_user'
    ui_test_password = self._generatePassword()
    cmd = (r'powershell -File c:\cel\supporting_files\enable_auto_logon.ps1 '
           r'-userName %s -password %s') % (ui_test_user, ui_test_password)
    self.RunCommand(instance_name, cmd)
    self._rebootInstance(instance_name)

    cmd = (r'powershell -File c:\cel\supporting_files\set_ui_agent.ps1 '
           '-username %s') % ui_test_user
    self.RunCommand(instance_name, cmd)
    self._rebootInstance(instance_name)

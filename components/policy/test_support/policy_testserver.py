# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A bare-bones test server for testing cloud policy support.

This implements a simple cloud policy test server that can be used to test
chrome's device management service client. The policy information is read from
the file named device_management in the server's data directory. It contains
enforced and recommended policies for the device and user scope, and a list
of managed users.

The format of the file is JSON. The root dictionary contains a list under the
key "managed_users". It contains auth tokens for which the server will claim
that the user is managed. The token string "*" indicates that all users are
claimed to be managed. Other keys in the root dictionary identify request
scopes. The user-request scope is described by a dictionary that holds two
sub-dictionaries: "mandatory" and "recommended". Both these hold the policy
definitions as key/value stores, their format is identical to what the Linux
implementation reads from /etc.
The device-scope holds the policy-definition directly as key/value stores in the
protobuf-format.

Example:

{
  "google/chromeos/device" : {
    "guest_mode_enabled" : false
  },
  "google/chromeos/user" : {
    "mandatory" : {
      "HomepageLocation" : "http://www.chromium.org",
      "IncognitoEnabled" : false
    },
     "recommended" : {
      "JavascriptEnabled": false
    }
  },
  "google/chromeos/publicaccount/user@example.com" : {
    "mandatory" : {
      "HomepageLocation" : "http://www.chromium.org"
    },
     "recommended" : {
    }
  },
  "managed_users" : [
    "secret123456"
  ],
  "current_key_index": 0,
  "robot_api_auth_code": "",
   "expected_errors": {
     "register": 500,
   }
   "allow_set_device_attributes" : false,
   "initial_enrollment_state": {
     "TEST_serial": {
       "initial_enrollment_mode": 2,
       "management_domain": "test-domain.com",
       "is_license_packaged_with_device": true
     }
   }
}

"""

import base64
from six.moves import BaseHTTPServer
from cryptography.hazmat.primitives.asymmetric import padding, rsa
from cryptography.hazmat.primitives import hashes, serialization
import glob
import google.protobuf.text_format
import hashlib
import json
import logging
import os
import random
import re
import six
import sys
import time
from six.moves import urllib
from six.moves.urllib import request as urllib_request
from six.moves.urllib import parse as urlparse

import testserver_base

import device_management_backend_pb2 as dm
import cloud_policy_pb2 as cp
import policy_common_definitions_pb2 as cd
import private_membership_pb2 as psm
import private_membership_rlwe_pb2 as psm_rlwe

# Policy for extensions is not supported on Android.
try:
  import chrome_extension_policy_pb2 as ep
except ImportError:
  ep = None

# Device policy is only available on Chrome OS builds.
try:
  import chrome_device_policy_pb2 as dp
except ImportError:
  dp = None

# pyopenssl is only reliably available on Chrome OS builds.
# This is currently OK because policy_testserver.py's support for certificate
# provisioning is only used in Tast test for now.
try:
  from OpenSSL import crypto
except ImportError:
  crypto = None

# List of machines that trigger the server to send kiosk enrollment response
# for the register request.
KIOSK_MACHINE_IDS = [ 'KIOSK' ]

# List of all IDs that will be used to construct PSM ID, and have membership.
PSM_MEMBERSHIP_SERIAL_NUMBER_IDS = [b"111111"]
PSM_MEMBERSHIP_BRAND_CODES = [b"TEST"]

# Dictionary containing base64-encoded policy signing keys plus per-domain
# signatures. Format is:
# {
#   'key': <base64-encoded PKCS8-format private key>,
#   'signatures': {
#     <domain1>: <base64-encdoded SHA256 signature for key + domain1>
#     <domain2>: <base64-encdoded SHA256 signature for key + domain2>
#     ...
#   }
# }
SIGNING_KEYS = [
    # Key1
    {'key':
       'MIIBVQIBADANBgkqhkiG9w0BAQEFAASCAT8wggE7AgEAAkEA2c3KzcPqvnJ5HCk3OZkf1'
       'LMO8Ht4dw4FO2U0EmKvpo0zznj4RwUdmKobH1AFWzwZP4CDY2M67MsukE/1Jnbx1QIDAQ'
       'ABAkBkKcLZa/75hHVz4PR3tZaw34PATlfxEG6RiRIwXlf/FFlfGIZOSxdW/I1A3XRl0/9'
       'nZMuctBSKBrcTRZQWfT/hAiEA9g8xbQbMO6BEH/XCRSsQbPlvj4c9wDtVEzeAzZ/ht9kC'
       'IQDiml+/lXS1emqml711jJcYJNYJzdy1lL/ieKogR59oXQIhAK+Pl4xa1U2VxAWpq7r+R'
       'vH55wdZT03hB4p2h4gvEzXBAiAkw9kvE0eZPiBZoRrrHIFTOH7FnnHlwBmV2+/2RsiVPQ'
       'IhAKqx/4qisivvmoM/xbzUagfoxwsu1A/4mGjhBKiS0BCq',
     'signatures':
       {'example.com':
          'l+sT5mziei/GbmiP7VtRCCfwpZcg7uKbW2OlnK5B/TTELutjEIAMdHduNBwbO44qOn'
          '/5c7YrtkXbBehaaDYFPGI6bGTbDmG9KRxhS+DaB7opgfCQWLi79Gn/jytKLZhRN/VS'
          'y+PEbezqMi3d1/xDxlThwWZDNwnhv9ER/Nu/32ZTjzgtqonSn2CQtwXCIILm4FdV/1'
          '/BdmZG+Ge4i4FTqYtInir5YFe611KXU/AveGhQGBIAXo4qYg1IqbVrvKBSU9dlI6Sl'
          '9TJJLbJ3LGaXuljgFhyMAl3gcy7ftC9MohEmwa+sc7y2mOAgYQ5SSmyAtQwQgAkX9J'
          '3+tfxjmoA/dg==',
        'chromepolicytest.com':
          'TzBiigZKwBdr6lyP6tUDsw+Q9wYO1Yepyxm0O4JZ4RID32L27sWzC1/hwC51fRcCvP'
          'luEVIW6mH+BFODXMrteUFWfbbG7jgV+Wg+QdzMqgJjxhNKFXPTsZ7/286LAd1vBY/A'
          'nGd8Wog6AhzfrgMbLNsH794GD0xIUwRvXUWFNP8pClj5VPgQnJrIA9aZwW8FNGbteA'
          'HacFB0T/oqP5s7XT4Qvkj14RLmCgTwEM8Vcpqy5teJaF8yN17wniveddoOQGH6s0HC'
          'ocprEccrH5fP/WVAPxCfx4vVYQY5q4CZ4K3f6dTC2FV4IDelM6dugEkvSS02YCzDaO'
          'N+Z7IwElzTKg==',
        'managedchrome.com':
          'T0wXC5w3GXyovA09pyOLX7ui/NI603UfbZXYyTbHI7xtzCIaHVPH35Nx4zdqVrdsej'
          'ErQ12yVLDDIJokY4Yl+/fj/zrkAPxThI+TNQ+jo0i+al05PuopfpzvCzIXiZBbkbyW'
          '3XfedxXP3IPN2XU2/3vX+ZXUNG6pxeETem64kGezkjkUraqnHw3JVzwJYHhpMcwdLP'
          'PYK6V23BbEHEVBtQZd/ledXacz7gOzm1zGni4e+vxA2roAdJWyhbjU0dTKNNUsZmMv'
          'ryQH9Af1Jw+dqs0RAbhcJXm2i8EUWIgNv6aMn1Z2DzZwKKjXsKgcYSRo8pdYa8RZAo'
          'UExd9roA9a5w==',
        }
     },
    # Key2
    {'key':
       'MIIBVAIBADANBgkqhkiG9w0BAQEFAASCAT4wggE6AgEAAkEAmZhreV04M3knCi6wibr49'
       'oDesHny1G33PKOX9ko8pcxAiu9ZqsKCj7wNW2PGqnLi81fddACwQtYn5xdhCtzB9wIDAQ'
       'ABAkA0z8m0cy8N08xundspoFZWO71WJLgv/peSDBYGI0RzJR1l9Np355EukQUQwRs5XrL'
       '3vRQZy2vDqeiR96epkAhRAiEAzJ4DVI8k3pAl7CGv5icqFkJ02viExIwehhIEXBcB6p0C'
       'IQDAKmzpoRpBEZRQ9xrTvPOi+Ea8Jnd478BU7CI/LFfgowIgMfLIoVWoDGRnvXKju60Hy'
       'xNB70oHLut9cADp64j6QMkCIDrgxN4QbmrhaAAmtiGKE1wrlgCwCIsVamiasSOKAqLhAi'
       'EAo/ItVcFtQPod97qG71CY/O4JzOciuU6AMhprs181vfM=',
     'signatures':
       # Key2 signatures
       {'example.com':
          'cO0nQjRptkeefKDw5QpJSQDavHABxUvbR9Wvoa235OG9Whw1RFqq2ye6pKnI3ezW6/'
          '7b4ANcpi5a7HV5uF8K7gWyYdxY8NHLeyrbwXxg5j6HAmHmkP1UZcf/dAnWqo7cW8g4'
          'DIQOhC43KkveMYJ2HnelwdXt/7zqkbe8/3Yj4nhjAUeARx86Sb8Nzydwkrvqs5Jw/x'
          '5LG+BODExrXXcGu/ubDlW4ivJFqfNUPQysqBXSMY2XCHPJDx3eECLGVVN/fFAWWgjM'
          'HFObAriAt0b18cc9Nr0mAt4Qq1oDzWcAHCPHE+5dr8Uf46BUrMLJRNRKCY7rrsoIin'
          '9Be9gs3W+Aww==',
        'chromepolicytest.com':
          'mr+9CCYvR0cTvPwlzkxqlpGYy55gY7cPiIkPAPoql51yHK1tkMTOSFru8Dy/nMt+0o'
          '4z7WO60F1wnIBGkQxnTj/DsO6QpCYi7oHqtLmZ2jsLQFlMyvPGUtpJEFvRwjr/TNbh'
          '6RqUtz1LQFuJQ848kBrx7nkte1L8SuPDExgx+Q3LtbNj4SuTdvMUBMvEERXiLuwfFL'
          'BefGjtsqfWETQVlJTCW7xcqOLedIX8UYgEDBpDOZ23A3GzCShuBsIut5m87R5mODht'
          'EUmKNDK1+OMc6SyDpf+r48Wph4Db1bVaKy8fcpSNJOwEgsrmH7/+owKPGcN7I5jYAF'
          'Z2PGxHTQ9JNA==',
        'managedchrome.com':
          'o5MVSo4bRwIJ/aooGyXpRXsEsWPG8fNA2UTG8hgwnLYhNeJCCnLs/vW2vdp0URE8jn'
          'qiG4N8KjbuiGw0rJtO1EygdLfpnMEtqYlFjrOie38sy92l/AwohXj6luYzMWL+FqDu'
          'WQeXasjgyY4s9BOLQVDEnEj3pvqhrk/mXvMwUeXGpbxTNbWAd0C8BTZrGOwU/kIXxo'
          'vAMGg8L+rQaDwBTEnMsMZcvlrIyqSg5v4BxCWuL3Yd2xvUqZEUWRp1aKetsHRnz5hw'
          'H7WK7DzvKepDn06XjPG9lchi448U3HB3PRKtCzfO3nD9YXMKTuqRpKPF8PeK11CWh1'
          'DBvBYwi20vbQ==',
       },
    },
]

INVALID_ENROLLMENT_TOKEN = 'invalid_enrollment_token'

POLICY_COMMON_DEFINITIONS_TYPES = [
  'StringList',
  'PolicyOptions',
  'BooleanPolicyProto',
  'IntegerPolicyProto',
  'StringPolicyProto',
  'StringListPolicyProto'
]

# Private key used for issuing certificates for issuing certificates
# for the built-in certificate provisioning feature.
CERT_PROVISIONING_CA_PRIVATE_KEY_PEM="""\
-----BEGIN RSA PRIVATE KEY-----
MIIEpAIBAAKCAQEAyh01PGm57kraP7TJpCtGEpvSx6OwKJmqCQp/kmr9hfFTrWW3
W4QgdmprzmYNt8vxtJkRLCz9/4K1lk1hfoDu5Qpx5VaNZ0bcwfzRG/PxcB9+XzZp
gppqpyq1HACYudqPi9wsxw1qSrB4Av1W+paOpdM6/blchwJpRuOmrb1iwooVYmR/
CnSDIEppPk5I8iwN3VTq3cSErDrC4Xtquw3jHs4aaq55ZcD31WRumiTkZY6oBZsH
gzFsy+uCqwfv8aIA6wU0nPo5tfq0xzIjDhAA/ZSkSx05UrEk1S7rgvpRIJMZ/+Pi
Jz/VX32XOBXyrGuwD+pS9zvtsPQ2rzz9aPhDFwIDAQABAoIBAH8tPdBT3rD43Lf1
dGQe7qrK7ii88R27A2lI99kUBY8AuVyEgonNa/fXIxru0Hb0l5TCNDIN5Y2fm8+F
xXEqhCgPGHfsrHFt/375LENgjm21A3m57U5HCBFEKE4EehWIV4bz9iESae2xePK4
osBveDcT4SzCNFynwcLfgIQWhUxPI7TlwEkR79vcM+l6CtbUVxUW+wTTS/Zp72FM
sBBNsXIBB0yHh0m30vg43jv3apBaZxogAPx2crWOu1A2NK20gQPgrCIHIjn1Of4e
JvWSwKFnmF9UzwDQ1KZo25EX4BEirVYYlk84Eq9Ds2ojOVD+mCEfbrEsqPorYl+c
F2d9CQECgYEA96kRVjhrS/Hbo5KNUOnOBs0uD8hCJ0cGlTRAvkwhGAENolewIuyj
wwXGfkAV0Rs3qf4HjzVeEiX1QegsHxqFPOhjuDK34pH6caU6UI1R6BAr9H6QmEO9
53LlKvlX+6kJ+RjXxvmftRFWgPhY526IbXgY4judI1+LR9FPmzyCLEECgYEA0OuD
Oc//mZomtnBVFiw0RiamERTMsWO6G7QVDs814utHaFVzJWkqVn6eIns5NgfyZ0ZE
xand7/wUtz0YM0wvdZPllhL0zXSujfqScO3qeE3XLPspv4dom+jdgwr1uR8rE5Av
8qeLrZaItSWDMKL2+1QX0Frn3k1cMO/wiw+w+VcCgYEAgtK5SL1W2HAzIK3anmJT
Jb6e1VFouIzJSmmmxZ87YA22YQpHDbvJKczUNH6vx5zEA7Uf0yNSxO1uJ9l37Ro6
RZlQi82m2zVXgU7RhhmQqbBZN7bftL8cArXrno7GTjbWANKBsSbNmX1GH6yQcfgu
cv0cz+zDrhrbXR2RGqSU8sECgYBmf3VVMsfq+ycNENWd2DgZRrLo5HR8fzn6h4Jh
TqXYW6gf9vRUIWFlKB+7OQtbh9CUfHQXKfy51cnwEGhEGpeaLuJPm6NA/YL6Izof
b4o+VapA5kSYM/3NqBStSv49QZ5nrbDocuzjUFxnyyyu+vUDX0GDtmXVucyGMeGo
yB0CZwKBgQDZ8RwInbSZbAo2/fjIxFGxxV0tSRH1n5L27QB5jzikXW+6jBOYRDQh
fHXdC808L+jJ0zgOBlJbbCM3TliiVqDE6Lcc3GShA1mrjvGmAy05e1ejgGZYX7c5
C97TFZS6CD+9uC2FV4RWJuO56kCGlDVLI3/iwIThtywvDt0qKnSsGA==
-----END RSA PRIVATE KEY-----"""

# The obfuscated_customer_id that will be served in device policy PolicyData
# responses.
OBFUSCATED_CUSTOMER_ID = 'policy_testserver_customer_id'

class PolicyRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  """Decodes and handles device management requests from clients.

  The handler implements all the request parsing and protobuf message decoding
  and encoding. It calls back into the server to lookup, register, and
  unregister clients.
  """

  def __init__(self, request, client_address, server):
    """Initialize the handler.

    Args:
      request: The request data received from the client as a string.
      client_address: The client address.
      server: The TestServer object to use for (un)registering clients.
    """
    BaseHTTPServer.BaseHTTPRequestHandler.__init__(self, request,
                                                   client_address, server)

  def GetUniqueParam(self, name):
    """Extracts a unique query parameter from the request.

    Args:
      name: Names the parameter to fetch.
    Returns:
      The parameter value or None if the parameter doesn't exist or is not
      unique.
    """
    if not hasattr(self, '_params'):
      self._params = urlparse.parse_qs(self.path[self.path.find('?') + 1:])

    param_list = self._params.get(name, [])
    if len(param_list) == 1:
      return param_list[0]
    return None

  def do_GET(self):
    """Handles GET requests.

    Currently this is only used to serve external policy data."""
    sep = self.path.find('?')
    path = self.path if sep == -1 else self.path[:sep]
    if path == '/externalpolicydata':
      http_response, raw_reply = self.HandleExternalPolicyDataRequest()
    elif path == '/configuration/test/exit':
      # This is not part of the standard DM server protocol.
      # This extension is added to make the test server exit gracefully
      # when the test is complete.
      self.server.stop = True
      http_response = 200
      raw_reply = b'OK'
    elif path == '/test/ping':
      # This path and reply are used by the test setup of host-driven tests for
      # Android to determine if the server is up, and are not part of the
      # DM protocol.
      http_response = 200
      raw_reply = b'Policy server is up.'
    else:
      http_response = 404
      raw_reply = b'Invalid path'
    self.send_response(http_response)
    self.end_headers()
    if six.PY3 and isinstance(raw_reply, str):
      raw_reply = raw_reply.encode()
    self.wfile.write(raw_reply)

  def do_POST(self):
    http_response, raw_reply = self.HandleRequest()
    self.send_response(http_response)
    if (http_response == 200):
      self.send_header('Content-Type', 'application/x-protobuffer')
    self.end_headers()
    if six.PY3 and isinstance(raw_reply, str):
      raw_reply = raw_reply.encode()
    self.wfile.write(raw_reply)

  def HandleExternalPolicyDataRequest(self):
    """Handles a request to download policy data for a component."""
    policy_key = self.GetUniqueParam('key')
    if not policy_key:
      return (400, b'Missing key parameter')
    data = self.server.ReadPolicyDataFromDataDir(policy_key)
    if data is None:
      return (404, b'Policy not found for ' + policy_key.encode('utf-8'))
    return (200, data)

  def HandleRequest(self):
    """Handles a request.

    Parses the data supplied at construction time and returns a pair indicating
    http status code and response data to be sent back to the client.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    rmsg = dm.DeviceManagementRequest()
    length = int(self.headers.get('content-length'))
    rmsg.ParseFromString(self.rfile.read(length))

    logging.debug('gaia auth token -> ' +
                  self.headers.get('Authorization', ''))
    logging.debug('oauth token -> ' + str(self.GetUniqueParam('oauth_token')))
    logging.debug('deviceid -> ' + str(self.GetUniqueParam('deviceid')))
    self.DumpMessage('Request', rmsg)

    request_type = self.GetUniqueParam('request')
    # Check server side requirements, as defined in
    # device_management_backend.proto.
    if (self.GetUniqueParam('devicetype') != '2' or
        self.GetUniqueParam('apptype') != 'Chrome' or
        (self.GetUniqueParam('deviceid') is not None and
         len(self.GetUniqueParam('deviceid')) >= 64)):
      return (400, 'Invalid request parameter')

    expected_error = self.GetExpectedError(request_type)
    if expected_error:
      return expected_error

    if request_type == 'register':
      response = self.ProcessRegister(rmsg.register_request)
    elif request_type == 'certificate_based_register':
      response = self.ProcessCertBasedRegister(
          rmsg.certificate_based_register_request)
    elif request_type == 'api_authorization':
      response = self.ProcessApiAuthorization(rmsg.service_api_access_request)
    elif request_type == 'unregister':
      response = self.ProcessUnregister(rmsg.unregister_request)
    elif request_type == 'policy':
      response = self.ProcessPolicy(rmsg, request_type)
    elif request_type == 'enterprise_check':
      response = self.ProcessAutoEnrollment(rmsg.auto_enrollment_request)
    elif request_type == 'enterprise_psm_check':
      response = self.ProcessPsmAutoEnrollment(
          rmsg.private_set_membership_request)
    elif request_type == 'device_initial_enrollment_state':
      response = self.ProcessDeviceInitialEnrollmentState(
          rmsg.device_initial_enrollment_state_request)
    elif request_type == 'device_state_retrieval':
      response = self.ProcessDeviceStateRetrievalRequest(
          rmsg.device_state_retrieval_request)
    elif request_type == 'status_upload':
      response = self.ProcessStatusUploadRequest(
          rmsg.device_status_report_request, rmsg.session_status_report_request)
    elif request_type == 'device_attribute_update_permission':
      response = self.ProcessDeviceAttributeUpdatePermissionRequest()
    elif request_type == 'device_attribute_update':
      response = self.ProcessDeviceAttributeUpdateRequest()
    elif request_type == 'remote_commands':
      response = self.ProcessRemoteCommandsRequest()
    elif request_type == 'check_android_management':
      response = self.ProcessCheckAndroidManagementRequest(
          rmsg.check_android_management_request,
          str(self.GetUniqueParam('oauth_token')))
    elif request_type == 'register_browser':
      response = self.ProcessRegisterBrowserRequest(
          rmsg.register_browser_request)
    elif request_type == 'chrome_desktop_report':
      response = self.ProcessChromeDesktopReportUploadRequest(
          rmsg.chrome_desktop_report_request)
    elif request_type == 'app_install_report':
      response = self.ProcessAppInstallReportRequest(
          rmsg.app_install_report_request)
    elif request_type == 'client_cert_provisioning':
      response = self.ProcessClientCertProvisioningRequest(
          rmsg.client_certificate_provisioning_request)
    else:
      return (400, 'Invalid request parameter')

    if isinstance(response[1], str):
      body = response[1]
    elif isinstance(response[1], google.protobuf.message.Message):
      self.DumpMessage('Response', response[1])
      body = response[1].SerializeToString()
    else:
      body = ''
    return (response[0], body)

  def CreatePolicyForExternalPolicyData(self, policy_key):
    """Returns an ExternalPolicyData protobuf for policy_key.

    If there is policy data for policy_key then the download url will be
    set so that it points to that data, and the appropriate hash is also set.
    Otherwise, the protobuf will be empty.

    Args:
      policy_key: The policy type and settings entity id, joined by '/'.

    Returns:
      A serialized ExternalPolicyData.
    """
    settings = ep.ExternalPolicyData()
    data = self.server.ReadPolicyDataFromDataDir(policy_key)
    if data:
      settings.download_url = urlparse.urljoin(
          self.server.GetBaseURL(), 'externalpolicydata?key=%s' % policy_key)
      settings.secure_hash = hashlib.sha256(data).digest()
      return settings.SerializeToString()
    else:
      return None

  def CheckGoogleLogin(self):
    """Extracts the auth token from the request and returns it. The token may
    either be a GoogleLogin token from an Authorization header, or an OAuth V2
    token from the oauth_token query parameter. Returns None if no token is
    present.
    """
    oauth_token = self.GetUniqueParam('oauth_token')
    if oauth_token:
      return oauth_token

    match = re.match('GoogleLogin auth=(\\w+)',
                     self.headers.get('Authorization', ''))
    if match:
      return match.group(1)

    return None

  def ProcessRegister(self, msg):
    """Handles a register request.

    Checks the query for authorization, device identifier and existence
    of PSM execution fields (if their expected values have been set in
    the config). Then, registers the device with the server and
    constructs a response.

    Args:
      msg: The DeviceRegisterRequest message received from the client.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    policy = self.server.GetPolicies()
    # Check the auth token and device ID.
    auth = self.CheckGoogleLogin()
    if not auth:
      return (403, 'No authorization')

    if ('managed_users' not in policy):
      return (500, 'Error in config - no managed users')
    username = self.server.ResolveUser(auth)
    if ('*' not in policy['managed_users'] and
        username not in policy['managed_users']):
      return (403, 'Unmanaged')

    # Checks for PSM execution fields.

    brand_machine_id = msg.brand_code + '_' + msg.machine_id
    psm_result_dict = self.server.GetPolicies().get('psm_result', {})
    psm_result = psm_result_dict.get(brand_machine_id, {})

    for field in psm_result:
      if not msg.HasField(field):
        return (400, 'DeviceRegisterRequest must have all required '
                'PSM execution fields')

      # The casting is necessary because int64_t fields won't have the same
      # equality otherwise.
      if int(psm_result[field]) != int(getattr(msg, field)):
        return (400, 'DeviceRegisterRequest must have all correct '
                'PSM execution values')

    return self.RegisterDeviceAndSendResponse(msg, username)

  def ProcessCertBasedRegister(self, signed_msg):
    """Handles a certificate based register request.

    Checks the query for the cert and device identifier, registers the
    device with the server and constructs a response.

    Args:
      msg: The CertificateBasedDeviceRegisterRequest message received from
           the client.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    # Unwrap the request
    try:
      req = self.UnwrapCertificateBasedDeviceRegistrationData(
          signed_msg.signed_request)
    except (IOError):
      return(400, 'Invalid request')

    if req.certificate_type != dm.CertificateBasedDeviceRegistrationData.\
        ENTERPRISE_ENROLLMENT_CERTIFICATE:
      return(403, 'Invalid certificate type for registration')

    return self.RegisterDeviceAndSendResponse(req.device_register_request, None)

  def RegisterDeviceAndSendResponse(self, msg, username):
    """Registers a device and send a response to the client.

    Checks that a device identifier was sent, registers the device
    with the server and constructs a response.
    """
    device_id = self.GetUniqueParam('deviceid')
    if not device_id:
      return (400, 'Missing device identifier')

    token_info = self.server.RegisterDevice(
        device_id, msg.machine_id, msg.type, username)

    # Send back the reply.
    response = dm.DeviceManagementResponse()
    response.register_response.device_management_token = (
        token_info['device_token'])
    response.register_response.machine_name = token_info['machine_name']
    response.register_response.enrollment_type = token_info['enrollment_mode']

    return (200, response)

  def UnwrapCertificateBasedDeviceRegistrationData(self, msg):
    """Verifies the signature of |msg| and if it is valid, return the
    certificate based device registration data. If not, throws an
    exception.

    Args:
      msg: SignedData received from the client.

    Returns:
      CertificateBasedDeviceRegistrationData
    """
    rdata = dm.CertificateBasedDeviceRegistrationData()
    rdata.ParseFromString(msg.data[:len(msg.data) - msg.extra_data_bytes])
    return rdata

  def ProcessApiAuthorization(self, msg):
    """Handles an API authorization request.

    Args:
      msg: The DeviceServiceApiAccessRequest message received from the client.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    policy = self.server.GetPolicies()

    # Return the auth code from the config file if it's defined. Default to an
    # empty auth code, which will instruct the enrollment flow to skip robot
    # auth setup.
    response = dm.DeviceManagementResponse()
    response.service_api_access_response.auth_code = policy.get(
        'robot_api_auth_code', '')

    return (200, response)

  def ProcessUnregister(self, msg):
    """Handles a register request.

    Checks for authorization, unregisters the device and constructs the
    response.

    Args:
      msg: The DeviceUnregisterRequest message received from the client.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    # Check the management token.
    token, response = self.CheckToken()
    if not token:
      return response

    # Unregister the device.
    self.server.UnregisterDevice(token['device_token'])

    # Prepare and send the response.
    response = dm.DeviceManagementResponse()
    response.unregister_response.CopyFrom(dm.DeviceUnregisterResponse())

    return (200, response)

  def ProcessPolicy(self, msg, request_type):
    """Handles a policy request.

    Checks for authorization, encodes the policy into protobuf representation
    and constructs the response.

    Args:
      msg: The DeviceManagementRequest message received from the client.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    token_info, error = self.CheckToken()
    if not token_info:
      return error

    key_update_request = msg.device_state_key_update_request
    if len(key_update_request.server_backed_state_keys) > 0:
      self.server.UpdateStateKeys(token_info['device_token'],
                                  key_update_request.server_backed_state_keys)

    # See whether the |username| for the client is known. During policy
    # validation, the client verifies that the policy blob is bound to the
    # appropriate user by comparing against this value. In case the server is
    # configured to resolve the actual user name from the access token via the
    # token info endpoint, the resolved |username| has been stored in
    # |token_info| when the client registered. If not, pass None as the
    # |username| in which case a value from the configuration file will be used.
    username = token_info.get('username')

    # If this is a |publicaccount| request, use the |settings_entity_id| from
    # the request as the |username|. This is required to validate policy for
    # extensions in device-local accounts.
    for request in msg.policy_request.requests:
      if request.policy_type == 'google/chromeos/publicaccount':
        username = request.settings_entity_id

    response = dm.DeviceManagementResponse()
    for request in msg.policy_request.requests:
      if (request.policy_type in
             ('google/android/user',
              'google/chromeos/device',
              'google/chromeos/publicaccount',
              'google/chromeos/user',
              'google/chrome/user',
              'google/chrome/machine-level-user')):
        fetch_response = response.policy_response.responses.add()
        self.ProcessCloudPolicy(request, token_info, fetch_response, username)
      elif (request.policy_type in
             ('google/chrome/extension',
              'google/chromeos/signinextension',
              'google/chrome/machine-level-extension')):
        self.ProcessCloudPolicyForExtensions(
            request, response.policy_response, token_info, username)
      else:
        fetch_response.error_code = 400
        fetch_response.error_message = 'Invalid policy_type'

    return (200, response)

  def ProcessAutoEnrollment(self, msg):
    """Handles an auto-enrollment check request.

    The reply depends on the value of the modulus:
      1: replies with no new modulus and corresponding sha256 hashes.
      2: replies with a new modulus, 4.
      4: replies with a new modulus, 2.
      8: fails with error 400.
      16: replies with a new modulus, 16.
      32: replies with a new modulus, 1.
      anything else: replies with no new modulus and an empty list of hashes

    These allow the client to pick the testing scenario its wants to simulate.

    Args:
      msg: The DeviceAutoEnrollmentRequest message received from the client.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    auto_enrollment_response = dm.DeviceAutoEnrollmentResponse()

    if msg.modulus == 1:
      if (msg.enrollment_check_type == dm.DeviceAutoEnrollmentRequest.
          ENROLLMENT_CHECK_TYPE_FRE):
        auto_enrollment_response.hashes.extend(
            self.server.GetMatchingStateKeyHashes(msg.modulus, msg.remainder))
      elif (msg.enrollment_check_type == dm.DeviceAutoEnrollmentRequest.
            ENROLLMENT_CHECK_TYPE_FORCED_ENROLLMENT):
        auto_enrollment_response.hashes.extend(
            self.server.GetMatchingSerialHashes(msg.modulus, msg.remainder))
    elif msg.modulus == 2:
      auto_enrollment_response.expected_modulus = 4
    elif msg.modulus == 4:
      auto_enrollment_response.expected_modulus = 2
    elif msg.modulus == 8:
      return (400, 'Server error')
    elif msg.modulus == 16:
      auto_enrollment_response.expected_modulus = 16
    elif msg.modulus == 32:
      auto_enrollment_response.expected_modulus = 1

    response = dm.DeviceManagementResponse()
    response.auto_enrollment_response.CopyFrom(auto_enrollment_response)
    return (200, response)

  def GetPsmMembershipResponse(self, encrypted_id):
    """Retrieves the PSM membership for a given encrypted_id.

    Args:
      encrypted_id: A string which contains an encrypted ID.

    Returns a boolean:
      1. True, if encrypted_id has a PSM membership.
      2. False, otherwise.
    """
    for serial_number in PSM_MEMBERSHIP_SERIAL_NUMBER_IDS:
      for brand_code in PSM_MEMBERSHIP_BRAND_CODES:
        psm_id = b'%s/%s' % (brand_code.hex().encode('ascii'), serial_number)
        if psm_id == encrypted_id:
          return True
    return False

  def GetPsmRlweOprfResponse(self, oprf_request):
    """Retrieves the fake PSM RLWE OPRF response for a given PSM OPRF request.

    Args:
      oprf_request: A PrivateMembershipRlweOprfRequest proto message.

    Returns:
    A PrivateMembershipRlweOprfResponse proto message which will include the
    passed encrypted_id, from the oprf_request, inside the
    doubly_encrypted_ids field.
    """
    oprf_response = psm_rlwe.PrivateMembershipRlweOprfResponse()
    encrypted_id = psm.DoublyEncryptedId()
    encrypted_id.queried_encrypted_id = oprf_request.encrypted_ids[0]
    oprf_response.doubly_encrypted_ids.append(encrypted_id)
    return oprf_response

  def GetPsmRlweQueryResponse(self, query_request):
    """Retrieves the fake PSM RLWE query response for a given PSM query request.

    Args:
      query_request: A PrivateMembershipRlweQueryRequest proto message.

    Returns:
    A PrivateMembershipRlweQueryResponse proto message which will include the
    following:
        1. The first passed encrypted_id, from queried_encrypted_id field,
        inside PrivateMembershipRlwePirResponse.queried_encrypted_id field.
        2. The membership response as a signal data inside
        PirResponse.plaintext_entry_size  field.
    """
    query_response = psm_rlwe.PrivateMembershipRlweQueryResponse()
    encrypted_id = query_request.queries[0].queried_encrypted_id
    pir_response = psm_rlwe.PrivateMembershipRlwePirResponse()
    pir_response.queried_encrypted_id = encrypted_id
    pir_response.pir_response.plaintext_entry_size =\
        self.GetPsmMembershipResponse(encrypted_id)
    query_response.pir_responses.append(pir_response)
    return query_response

  def ProcessPsmAutoEnrollment(self, msg):
    """Handles an auto-enrollment PSM check request.

    The reply depends on which PSM request phases is received, as follows:
      1. In case RLWE OPRF request is filled, replies by sending OPRF response
      that contain the first received encrypted id.
      2. In case RLWE Query request is filled, replies by sending Query response
      that contain the first queried encrypted id. Also, sending out a signal
      data (inside PirResponse.plaintext_entry_size) to indicate whether there
      is a membership or not, depending on the received encrypted id.
      3. Otherwise, it will return an error.

    These allow the client to pick the testing scenario it wants to simulate.

    Args:
      msg: The PrivateSetMembershipRequest message received from the client.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    psm_response = dm.PrivateSetMembershipResponse()

    rlwe_request = msg.rlwe_request

    if rlwe_request.HasField('oprf_request'):
      oprf_request = rlwe_request.oprf_request
      if not len(oprf_request.encrypted_ids):
        return (400, 'PSM RLWE OPRF request must contains encrypted_ids field')
      psm_response.rlwe_response.oprf_response.CopyFrom(
          self.GetPsmRlweOprfResponse(oprf_request))
    elif rlwe_request.HasField('query_request'):
      query_request = rlwe_request.query_request
      if not len(query_request.queries):
        return (400, 'PSM RLWE query request must contains queries field')
      psm_response.rlwe_response.query_response.CopyFrom(
          self.GetPsmRlweQueryResponse(query_request))
    else:
      return (400,
              'PSM RLWE oprf_request, or query_request fields must be filled')

    response = dm.DeviceManagementResponse()
    response.private_set_membership_response.CopyFrom(psm_response)
    return (200, response)

  def ProcessDeviceInitialEnrollmentState(self, msg):
    """Handles a device initial enrollment state request.

    Response data is taken from server configuration.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    device_initial_enrollment_state_response = (
        dm.DeviceInitialEnrollmentStateResponse())

    brand_serial_id = msg.brand_code + '_' + msg.serial_number;
    initial_state_dict = (self.server.GetPolicies().
                          get('initial_enrollment_state', {}))
    state = initial_state_dict.get(brand_serial_id, {})

    FIELDS = [
        'initial_enrollment_mode',
        'management_domain',
        'is_license_packaged_with_device',
    ]
    for field in FIELDS:
      if field in state:
        setattr(device_initial_enrollment_state_response, field, state[field])

    response = dm.DeviceManagementResponse()
    response.device_initial_enrollment_state_response.CopyFrom(
        device_initial_enrollment_state_response)
    return (200, response)

  def ProcessDeviceStateRetrievalRequest(self, msg):
    """Handles a device state retrieval request.

    Response data is taken from server configuration.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    device_state_retrieval_response = dm.DeviceStateRetrievalResponse()

    client = self.server.LookupByStateKey(msg.server_backed_state_key)
    if client is not None:
      state = self.server.GetPolicies().get('device_state', {})
      FIELDS = [
          'management_domain',
          'restore_mode',
      ]
      for field in FIELDS:
        if field in state:
          setattr(device_state_retrieval_response, field, state[field])

    response = dm.DeviceManagementResponse()
    response.device_state_retrieval_response.CopyFrom(
        device_state_retrieval_response)
    return (200, response)

  def ProcessStatusUploadRequest(self, device_status, session_status):
    """Handles a device/session status upload request.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    # Empty responses indicate a successful upload.
    device_status_report_response = dm.DeviceStatusReportResponse()
    session_status_report_response = dm.SessionStatusReportResponse()

    response = dm.DeviceManagementResponse()
    response.device_status_report_response.CopyFrom(
        device_status_report_response)
    response.session_status_report_response.CopyFrom(
        session_status_report_response)

    return (200, response)

  def ProcessDeviceAttributeUpdatePermissionRequest(self):
    """Handles a device attribute update permission request.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    response = dm.DeviceManagementResponse()
    policy = self.server.GetPolicies()
    update_allowed = True
    if ('allow_set_device_attributes' in policy):
      update_allowed = policy['allow_set_device_attributes']

    response.device_attribute_update_permission_response.result = (
        dm.DeviceAttributeUpdatePermissionResponse.ATTRIBUTE_UPDATE_ALLOWED
        if update_allowed else
        dm.DeviceAttributeUpdatePermissionResponse.ATTRIBUTE_UPDATE_DISALLOWED)

    return (200, response)

  def ProcessDeviceAttributeUpdateRequest(self):
    """Handles a device attribute update request.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    response = dm.DeviceManagementResponse()
    response.device_attribute_update_response.result = (
        dm.DeviceAttributeUpdateResponse.ATTRIBUTE_UPDATE_SUCCESS)

    return (200, response)

  def ProcessRemoteCommandsRequest(self):
    """Handles a remote command request.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    return (200, '')

  def ProcessCheckAndroidManagementRequest(self, msg, oauth_token):
    """Handles a check Android management request.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    check_android_management_response = dm.CheckAndroidManagementResponse()

    response = dm.DeviceManagementResponse()
    response.check_android_management_response.CopyFrom(
        check_android_management_response)
    if oauth_token == 'managed-auth-token':
      return (409, response)
    elif oauth_token == 'unmanaged-auth-token':
      return (200, response)
    else:
      return (403, response)

  def ProcessAppInstallReportRequest(self, app_install_report):
    """Handles a push-installed app report upload request.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    app_install_report_response = dm.AppInstallReportResponse()
    response = dm.DeviceManagementResponse()
    response.app_install_report_response.CopyFrom(app_install_report_response)

    return (200, response)

  def ProcessRegisterBrowserRequest(self, msg):
    """Handles a browser registration request.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    enrollment_token = None
    match = re.match('GoogleEnrollmentToken token=(\\w+)',
                     self.headers.get('Authorization', ''))
    if match:
      enrollment_token = match.group(1)
    if not enrollment_token:
      return (401, 'Missing enrollment token.')

    device_id = self.GetUniqueParam('deviceid')
    if not device_id:
      return (400, 'Parameter deviceid is missing.')

    if not msg.machine_name and not msg.device_model:
      return (400, 'Either machine name or device model must be non-empty.')

    if enrollment_token == INVALID_ENROLLMENT_TOKEN:
      return (401, 'Invalid enrollment token')

    dm_token = 'fake_device_management_token'
    response = dm.DeviceManagementResponse()
    response.register_response.device_management_token = (
        dm_token)
    self.server.RegisterBrowser(dm_token, device_id, msg.machine_name)

    return (200, response)

  def ProcessChromeDesktopReportUploadRequest(self, chrome_desktop_report):
    """Handles a chrome desktop report upload request.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    # Empty responses indicate a successful upload.
    chrome_desktop_report_response = dm.ChromeDesktopReportResponse()

    response = dm.DeviceManagementResponse()
    response.chrome_desktop_report_response.CopyFrom(
        chrome_desktop_report_response)

    return (200, response)

  def SetProtoRepeatedField(self, group_message, field, field_value):
    assert type(field_value) == list
    entries = group_message.__getattribute__(field.name)
    if field.message_type is None:
      for list_item in field_value:
        entries.append(list_item)
    else:
      # This field is itself a protobuf.
      sub_type = field.message_type
      for sub_value in field_value:
        assert type(sub_value) == dict
        # Add a new sub-protobuf per list entry.
        sub_message = entries.add()
        # Now iterate over its fields and recursively add them.
        for sub_field in sub_message.DESCRIPTOR.fields:
          if sub_field.name in sub_value:
            sub_field_value = sub_value[sub_field.name]
            self.SetProtobufMessageField(sub_message,
                                         sub_field, sub_field_value)

  def SetProtoMessageField(self, group_message, field, field_value):
    if field.message_type.name == 'StringList':
      assert type(field_value) == list
      entries = group_message.__getattribute__(field.name).entries
      for list_item in field_value:
        entries.append(list_item)
    else:
      assert type(field_value) == dict
      sub_message = group_message.__getattribute__(field.name)
      for sub_field in sub_message.DESCRIPTOR.fields:
        if sub_field.name in field_value:
          sub_field_value = field_value[sub_field.name]
          self.SetProtobufMessageField(sub_message, sub_field, sub_field_value)

  def SetProtoField(self, group_message, field, field_value):
    if field.type == field.TYPE_BOOL:
      assert type(field_value) == bool
    elif field.type == field.TYPE_STRING:
      if six.PY3:
        assert isinstance(field_value, str)
      else:
        assert type(field_value) in [str, unicode]
    elif field.type == field.TYPE_BYTES:
      if six.PY3:
        assert isinstance(field_value, str)
      else:
        assert type(field_value) in [str, unicode]
      field_value = bytes.fromhex(field_value)
    elif (field.type == field.TYPE_INT64 or
          field.type == field.TYPE_INT32 or
          field.type == field.TYPE_ENUM):
      assert type(field_value) == int
    else:
      return False
    setattr(group_message, field.name, field_value)
    return True

  def SetProtobufMessageField(self, group_message, field, field_value):
    """Sets a field in a protobuf message.

    Args:
      group_message: The protobuf message.
      field: The field of the message to set, it should be a member of
          group_message.DESCRIPTOR.fields.
      field_value: The value to set.
    """
    if field.label == field.LABEL_REPEATED:
      self.SetProtoRepeatedField(group_message, field, field_value)
    elif field.type == field.TYPE_MESSAGE:
      self.SetProtoMessageField(group_message, field, field_value)
    elif not self.SetProtoField(group_message, field, field_value):
      raise Exception('Unknown field type %s' % field.type)

  def GatherExtensionPolicySettings(self, settings, policies):
    """Copies all the policies from a dictionary into a protobuf of type
    ExternalPolicyData.

    Args:
      settings: The destination: a ExternalPolicyData protobuf.
      policies: The source: a dictionary containing the extension policies.
    """
    for field in settings.DESCRIPTOR.fields:
      # |field| is the entry for a specific policy in the top-level
      # ExternalPolicyData proto.
      field_value = policies.get(field.name)
      if field_value is None:
        continue

      field_descriptor = settings.DESCRIPTOR.fields_by_name[field.name]
      self.SetProtobufMessageField(settings, field_descriptor,
                                   field_value)

  def GetMessageDefinitionSource(self, message_type):
    """Retrieve either policy_common_defintions, or chrome_device_policy
    proto file, which contains the definition of the message.

    Args:
      message_type: name of the message definition type.
    """
    if message_type in POLICY_COMMON_DEFINITIONS_TYPES:
      return 'cd'
    return 'dp'

  def GatherDevicePolicySettings(self, settings, policies):
    """Copies all the policies from a dictionary into a protobuf of type
    CloudDeviceSettingsProto.

    Args:
      settings: The destination ChromeDeviceSettingsProto protobuf.
      policies: The source dictionary containing policies in JSON format.
    """
    for group in settings.DESCRIPTOR.fields:
      # Create protobuf message for group.
      group_message = eval(self.GetMessageDefinitionSource(
          group.message_type.name) + '.' + group.message_type.name + '()')
      # Indicates if at least one field was set in |group_message|.
      got_fields = False
      # Iterate over fields of the message and feed them from the
      # policy config file.
      for field in group_message.DESCRIPTOR.fields:
        field_value = None
        full_name = '{}.{}'.format(group.name, field.name)
        if full_name in policies:
          got_fields = True
          field_value = policies[full_name]
          self.SetProtobufMessageField(group_message, field, field_value)
        elif field.name in policies:
          got_fields = True
          field_value = policies[field.name]
          self.SetProtobufMessageField(group_message, field, field_value)
      if got_fields:
        settings.__getattribute__(group.name).CopyFrom(group_message)

  def GatherUserPolicySettings(self, settings, policies):
    """Copies all the policies from a dictionary into a protobuf of type
    CloudPolicySettings.

    Args:
      settings: The destination: a CloudPolicySettings protobuf.
      policies: The source: a dictionary containing policies under keys
          'recommended' and 'mandatory'.
    """
    for field in settings.DESCRIPTOR.fields:
      # |field| is the entry for a specific policy in the top-level
      # CloudPolicySettings proto.

      # Look for this policy's value in the mandatory or recommended dicts.
      if field.name in policies.get('mandatory', {}):
        mode = cd.PolicyOptions.MANDATORY
        value = policies['mandatory'][field.name]
      elif field.name in policies.get('recommended', {}):
        mode = cd.PolicyOptions.RECOMMENDED
        value = policies['recommended'][field.name]
      else:
        continue

      # Create protobuf message for this policy.
      policy_message = eval('cd.' + field.message_type.name + '()')
      policy_message.policy_options.mode = mode
      field_descriptor = policy_message.DESCRIPTOR.fields_by_name['value']
      self.SetProtobufMessageField(policy_message, field_descriptor, value)
      settings.__getattribute__(field.name).CopyFrom(policy_message)

  def ProcessCloudPolicyForExtensions(self, request, response, token_info,
                                      username=None):
    """Handles a request for policy for extensions.

    A request for policy for extensions is slightly different from the other
    cloud policy requests, because it can trigger 0, one or many
    PolicyFetchResponse messages in the response.

    Args:
      request: The PolicyFetchRequest that triggered this handler.
      response: The DevicePolicyResponse message for the response. Multiple
      PolicyFetchResponses will be appended to this message.
      token_info: The token extracted from the request.
      username: The username for the response. May be None.
    """
    # Send one PolicyFetchResponse for each extension that has
    # configuration data at the server.
    ids = self.server.ListMatchingComponents(request.policy_type)
    if not ids:
      # Fetch the ids from the policy JSON, if none in the config directory.
      policy = self.server.GetPolicies()
      ext_policies = policy.get(request.policy_type, {})
      ids = list(ext_policies.keys())

    for settings_entity_id in ids:
      # Reuse the extension policy request, to trigger the same signature
      # type in the response.
      request.settings_entity_id = settings_entity_id
      fetch_response = response.responses.add()
      self.ProcessCloudPolicy(request, token_info, fetch_response, username)
      # Don't do key rotations for these messages.
      fetch_response.ClearField('new_public_key')
      fetch_response.ClearField('new_public_key_signature')
      fetch_response.ClearField(
          'new_public_key_verification_signature_deprecated')

  def ProcessCloudPolicy(self, msg, token_info, response, username=None):
    """Handles a cloud policy request. (New protocol for policy requests.)

    Encodes the policy into protobuf representation, signs it and constructs
    the response.

    Args:
      msg: The CloudPolicyRequest message received from the client.
      token_info: The token extracted from the request.
      response: A PolicyFetchResponse message that should be filled with the
                response data.
      username: The username for the response. May be None.
    """

    # Response is only given if the scope is specified in the config file.
    # Normally 'google/chromeos/device', 'google/chromeos/user' and
    # 'google/chromeos/publicaccount' should be accepted.
    policy = self.server.GetPolicies()
    policy_value = ''
    policy_key = msg.policy_type
    if msg.settings_entity_id:
      policy_key += '/' + msg.settings_entity_id
    if msg.policy_type in token_info['allowed_policy_types']:
      if msg.policy_type in ('google/android/user',
                             'google/chromeos/publicaccount',
                             'google/chromeos/user',
                             'google/chrome/user',
                             'google/chrome/machine-level-user'):
        settings = cp.CloudPolicySettings()
        payload = self.server.ReadPolicyFromDataDir(policy_key, settings)
        if payload is None:
          self.GatherUserPolicySettings(settings, policy.get(policy_key, {}))
          payload = settings.SerializeToString()
      elif msg.policy_type == 'google/chromeos/device':
        settings = dp.ChromeDeviceSettingsProto()
        payload = self.server.ReadPolicyFromDataDir(policy_key, settings)
        if payload is None:
          self.GatherDevicePolicySettings(settings, policy.get(policy_key, {}))
          payload = settings.SerializeToString()
      elif msg.policy_type in ('google/chrome/extension',
                               'google/chromeos/signinextension',
                               'google/chrome/machine-level-extension'):
        settings = ep.ExternalPolicyData()
        payload = self.server.ReadPolicyFromDataDir(policy_key, settings)
        if payload is None:
          payload = self.CreatePolicyForExternalPolicyData(policy_key)
        if payload is None:
          ext_policies = policy.get(msg.policy_type, {})
          policies = ext_policies.get(msg.settings_entity_id, {})
          self.GatherExtensionPolicySettings(settings, policies)
          payload = settings.SerializeToString()
      else:
        response.error_code = 400
        response.error_message = 'Invalid policy type'
        return
    else:
      response.error_code = 400
      response.error_message = 'Request not allowed for the token used'
      return

    # Determine the current key on the client.
    client_key_version = None
    client_key = None
    if msg.HasField('public_key_version'):
      client_key_version = msg.public_key_version
      client_key = self.server.GetKeyByVersion(client_key_version)
      if client_key is None:
        response.error_code = 400
        response.error_message = 'Invalid public key version'
        return

    # Choose the key for signing the policy.
    signing_key_version = self.server.GetKeyVersionForSigning(
        client_key_version)
    signing_key = self.server.GetKeyByVersion(signing_key_version)
    assert signing_key is not None

    # Fill the policy data protobuf.
    policy_data = dm.PolicyData()
    policy_data.policy_type = msg.policy_type
    policy_data.timestamp = int(policy.get('timestamp', time.time() * 1000))
    policy_data.request_token = token_info['device_token']
    policy_data.policy_value = payload
    policy_data.machine_name = token_info['machine_name']
    policy_data.settings_entity_id = msg.settings_entity_id
    policy_data.service_account_identity = policy.get(
        'service_account_identity',
        'policy_testserver.py-service_account_identity@gmail.com')

    policy_invalidation_topic = policy.get('policy_invalidation_topic')
    if policy_invalidation_topic is not None:
      policy_data.policy_invalidation_topic = \
          policy_invalidation_topic.encode('ascii')

    if msg.signature_type != dm.PolicyFetchRequest.NONE:
      policy_data.public_key_version = signing_key_version

    if username:
      policy_data.username = username
    else:
      # If the correct |username| is unknown, rely on a manually-configured
      # username from the configuration file or use a default.
      policy_data.username = policy.get('policy_user', 'username@example.com')
    policy_data.device_id = token_info['device_id']

    # Set affiliation IDs so that user was managed on the device.
    device_affiliation_ids = policy.get('device_affiliation_ids')
    if device_affiliation_ids:
      policy_data.device_affiliation_ids.extend(device_affiliation_ids)

    user_affiliation_ids = policy.get('user_affiliation_ids')
    if user_affiliation_ids:
      policy_data.user_affiliation_ids.extend(user_affiliation_ids)

    if msg.policy_type == 'google/chromeos/device':
      # Fill |obfuscated_customer_id| for PolicyData in device policy fetches.
      # Verified Access attestation using the Enterprise Machine Key (EMK)
      # requires it since https://crbug.com/1073974.
      policy_data.obfuscated_customer_id = OBFUSCATED_CUSTOMER_ID

    response.policy_data = policy_data.SerializeToString()

    # Sign the serialized policy data
    if msg.signature_type == dm.PolicyFetchRequest.SHA1_RSA:
      response.policy_data_signature = signing_key['private_key'].sign(
          response.policy_data, padding.PKCS1v15(), hashes.SHA1())
      if msg.public_key_version != signing_key_version:
        response.new_public_key = signing_key['public_key']

        # Set the verification signature appropriate for the policy domain.
        if 'signatures' in signing_key:
          verification_sig = self.GetSignatureForDomain(
              signing_key['signatures'], policy_data.username)

          if verification_sig:
            assert len(verification_sig) == 256, \
                'bad signature size: %d' % len(verification_sig)
            response.new_public_key_verification_signature_deprecated = (
                verification_sig)

        if client_key is not None:
          response.new_public_key_signature = client_key['private_key'].sign(
              response.new_public_key, padding.PKCS1v15(), hashes.SHA1())

    return (200, response.SerializeToString())

  def ProcessClientCertProvisioningRequest(self, msg):
    """Handles a client certificate provisioning request.

    Issues a client certificate generated with the public key provided and
    sends it back to the requesting client.

    Args:
      msg: The ClientCertificateProvisioningRequest message received from the
        client. Contains one of start_csr_request, finish_csr_request, or
        download_cert_request.

    Returns:
      A tuple of HTTP status code and a ClientCertificateProvisioningResponse
      message that is filled with hard coded data and in the case of a
      download_cert_request in addition with the generated certificate.
    """

    if crypto == None:
      return (400, 'Could not find pyopenssl.')

    if msg.HasField('start_csr_request'):
      start_csr_response = dm.StartCsrResponse()

      # real but outdated b64 encoded verified access challenge received from
      # the Enterprise Verified Access Test extension
      va_challenge_b64 = (
        'CkEKFkVudGVycHJpc2VLZXlDaGFsbGVuZ2USIO6YSl1AvTjbEvRukIFMF2pA4AwCc1w4f'
        'ZX3n3sGcLInGOPh+IWKLhKAAm/WHGk7ahCjPk4IXLfDlUUmmZdfW1scNcwkKk/x24Znvb'
        'T7tyrmxLzO5nG69ycW7f+2bacbtfGlf0UOGeljcqBIIoHjJPlm0d2gCTa2msghS9ovaSg'
        '/wbY5DPeNkcG5drDq5Es5hzlZ49Bhvv5cAbDDsGNobPJQ3ojbu/mrdlb3mlB1oNTmbfoP'
        'TBrr6n9JXvywsJmHyInTySiFPOR8TT1cQoDA0pZ0ccHMJfLia1/FCW/pGpI6GpSzCQLq2'
        'hH0cFVuef3lGn09EeUHTPejbm6gcrHe9VDAFXMI8SzUlgMBBjHtTpo9GXJbwkTrGFXdkE'
        'U5BY1KukrsIVqdmAGFTDM='
      )

      va_challenge = base64.b64decode(va_challenge_b64)
      start_csr_response.invalidation_topic = 'invalidation_topic_123'
      start_csr_response.va_challenge = va_challenge
      start_csr_response.hashing_algorithm = 2
      start_csr_response.signing_algorithm = 1
      start_csr_response.data_to_sign = b'data_to_sign_123'

      response = dm.DeviceManagementResponse()
      response.client_certificate_provisioning_response.start_csr_response.\
          CopyFrom(start_csr_response)
      return (200, response)

    if msg.HasField('finish_csr_request'):
      finish_csr_response = dm.FinishCsrResponse()

      response = dm.DeviceManagementResponse()
      response.client_certificate_provisioning_response.finish_csr_response.\
          CopyFrom(finish_csr_response)
      return (200, response)

    if msg.HasField('download_cert_request'):
      download_cert_response = dm.DownloadCertResponse()

      pubKey = crypto.load_publickey(crypto.FILETYPE_ASN1, msg.public_key)

      caPrivKey = crypto.load_privatekey(
        crypto.FILETYPE_PEM, CERT_PROVISIONING_CA_PRIVATE_KEY_PEM, b'pass')

      cert = crypto.X509()
      cert.set_serial_number(3)
      cert.gmtime_adj_notBefore(0)
      cert.gmtime_adj_notAfter(365*24*60*60)
      cert.get_subject().CN = "TastTest"
      cert.set_issuer(cert.get_subject())
      cert.set_issuer(cert.get_subject())
      cert.set_pubkey(pubKey)
      cert.sign(caPrivKey, 'sha256')

      download_cert_response.pem_encoded_certificate  =\
        crypto.dump_certificate(crypto.FILETYPE_PEM, cert)

      response = dm.DeviceManagementResponse()
      response.client_certificate_provisioning_response.\
        download_cert_response.CopyFrom(download_cert_response)
      return (200, response)

    return (400, 'Invalid request parameter')

  def GetSignatureForDomain(self, signatures, username):
    parsed_username = username.split("@", 1)
    if len(parsed_username) != 2:
      logging.error('Could not extract domain from username: %s' % username)
      return None
    domain = parsed_username[1]

    # Lookup the domain's signature in the passed dictionary. If none is found,
    # fallback to a wildcard signature.
    if domain in signatures:
      return signatures[domain]
    if '*' in signatures:
      return signatures['*']

    # No key matching this domain.
    logging.error('No verification signature matching domain: %s' % domain)
    return None

  def CheckToken(self):
    """Helper for checking whether the client supplied a valid DM token.

    Extracts the token from the request and passed to the server in order to
    look up the client.

    Returns:
      A pair of token information record and error response. If the first
      element is None, then the second contains an error code to send back to
      the client. Otherwise the first element is the same structure that is
      returned by LookupToken().
    """
    error = 500
    dmtoken = None
    request_device_id = self.GetUniqueParam('deviceid')
    match = re.match('GoogleDMToken token=(\\w+)',
                     self.headers.get('Authorization', ''))
    if match:
      dmtoken = match.group(1)
    if not dmtoken:
      error = 401
    else:
      token_info = self.server.LookupToken(dmtoken)
      if (not token_info or
          not request_device_id or
          token_info['device_id'] != request_device_id):
        error = 410
      else:
        return (token_info, None)

    logging.debug('Token check failed with error %d' % error)

    return (None, (error, 'Server error %d' % error))

  def DumpMessage(self, label, msg):
    """Helper for logging an ASCII dump of a protobuf message."""
    logging.debug('%s\n%s' % (label, str(msg)))

  def GetExpectedError(self, request):
    """
    Returns the preset HTTP error for |request| if it is defined in
    configuration.

    Returns:
      A tuple of HTTP status code and response data to send to the client or
      None if no error was defined.
    """
    policy = self.server.GetPolicies()
    if 'request_errors' in policy:
      errors = policy['request_errors']
      if (request in errors) and (errors[request] > 0):
        return errors[request], 'Preconfigured error'
    return None

class PolicyTestServer(testserver_base.BrokenPipeHandlerMixIn,
                       testserver_base.StoppableHTTPServer):
  """Handles requests and keeps global service state."""

  def __init__(self, server_address, data_dir, policy_path, client_state_file,
               private_key_paths, rotate_keys_automatically, server_base_url):
    """Initializes the server.

    Args:
      server_address: Server host and port.
      data_dir: Directory that contains files with signature, policy, clients
        information.
      policy_path: Names the file to read JSON-formatted policy from.
      client_state_file: Path to file with registered clients.
      private_key_paths: List of paths to read private keys from.
      rotate_keys_automatically: Whether the keys should be rotated in a
        round-robin fashion for each policy request (by default, either the
        key specified in the config or the first key will be used for all
        requests).
      server_base_url: The server base URL. Used for ExternalPolicyData message.
    """
    testserver_base.StoppableHTTPServer.__init__(self, server_address,
                                                 PolicyRequestHandler)
    self.data_dir = data_dir
    self.policy_path = policy_path
    self.rotate_keys_automatically = rotate_keys_automatically
    self.server_base_url = server_base_url

    #  _registered_tokens and client_state_file kept in sync if the file is set.
    self._registered_tokens = {}
    self.client_state_file = client_state_file
    self.client_state_file_timestamp = 0

    self.keys = []
    if private_key_paths:
      # Load specified keys from the filesystem.
      for key_path in private_key_paths:
        try:
          key_str = open(key_path, 'rb').read()
        except IOError:
          print('Failed to load private key from %s' % key_path)
          continue
        try:
          key = serialization.load_pem_private_key(key_str, password=None)
        except ValueError:
          key = serialization.load_der_private_key(key_str, password=None)

        assert key is not None
        if not isinstance(key, rsa.RSAPrivateKey):
          raise TypeError('Unexpected key type')

        key_info = { 'private_key' : key }

        # Now try to read in a signature, if one exists.
        try:
          key_sig = open(key_path + '.sig', 'rb').read()
          # Create a dictionary with the wildcard domain + signature
          key_info['signatures'] = {'*': key_sig}
        except IOError:
          print('Failed to read validation signature from %s.sig' % key_path)
        self.keys.append(key_info)
    else:
      # Use the canned private keys if none were passed from the command line.
      for signing_key in SIGNING_KEYS:
        decoded_key = base64.b64decode(signing_key['key']);
        key = serialization.load_der_private_key(decoded_key, password=None)
        assert key is not None
        assert isinstance(key, rsa.RSAPrivateKey)
        # Grab the signature dictionary for this key and decode all of the
        # signatures.
        signature_dict = signing_key['signatures']
        decoded_signatures = {}
        for domain in signature_dict:
          decoded_signatures[domain] = base64.b64decode(signature_dict[domain])
        self.keys.append({'private_key': key,
                          'signatures': decoded_signatures})

    # Derive the public keys from the private keys.
    for entry in self.keys:
      entry['public_key'] = entry['private_key'].public_key().public_bytes(
          encoding=serialization.Encoding.DER,
          format=serialization.PublicFormat.SubjectPublicKeyInfo)

    try:
      self.ReadClientStateFile()
    except Exception as e:
      # Could fail if file is not written yet.
      logging.info('failed to load client state %s' % e)

  def GetPolicies(self):
    """Returns the policies to be used, reloaded from the backend file every
       time this is called.
    """
    policy = {}
    if json is None:
      logging.error('No JSON module, cannot parse policy information')
    elif not os.path.exists(self.policy_path):
      logging.warning('Missing policies file %s' % self.policy_path)
    else:
      try:
        policy = json.loads(open(self.policy_path).read(), strict=False)
      except IOError:
        logging.error('Failed to load policies from %s' % self.policy_path)
    return policy

  def GetKeyByVersion(self, key_version):
    """Obtains the object containing key properties, given the key version.

    Args:
      key_version: Integer key version.

    Returns:
      The object containing key properties, or None if the key is not found.
    """
    # Convert the policy key version, which has to be positive according to the
    # policy protocol definition, to an index in the keys list.
    key_index = key_version - 1
    if key_index < 0:
      return None
    if key_index >= len(self.keys):
      if self.rotate_keys_automatically:
        key_index %= len(self.keys)
      else:
        return None
    return self.keys[key_index]

  def GetKeyVersionForSigning(self, client_key_version):
    """Determines the version of the key that should be used for signing policy.

    Args:
      client_key_version: Either an integer representing the current key version
        provided by the client, or None if the client didn't provide any.

    Returns:
      An integer representing the signing key version.
    """
    if self.rotate_keys_automatically and client_key_version is not None:
      # Return the incremented version, which means that the key should be
      # rotated.
      return client_key_version + 1
    # Return the version that is specified by the config, defaulting to using
    # the very first key. Note that incrementing here is done due to conversion
    # between indices in the keys list and the key versions transmitted to the
    # client (where the latter have to be positive according to the policy
    # protocol definition).
    return self.GetPolicies().get('current_key_index', 0) + 1

  def ResolveUser(self, auth_token):
    """Tries to resolve an auth token to the corresponding user name.

    If enabled, this makes a request to the token info endpoint to determine the
    user ID corresponding to the token. If token resolution is disabled or the
    request fails, this will return the policy_user config parameter.
    """
    config = self.GetPolicies()
    token_info_url = config.get('token_info_url')
    if token_info_url is not None:
      try:
        token_info = urllib_request.urlopen(token_info_url + '?' +
            urlparse.urlencode({'access_token': auth_token})).read()
        return json.loads(token_info)['email']
      except Exception as e:
        logging.info('Failed to resolve user: %s', e)

    return config.get('policy_user')

  def RegisterDevice(self, device_id, machine_id, type, username):
    """Registers a device or user and generates a DM token for it.

    Args:
      device_id: The device identifier provided by the client.

    Returns:
      The newly generated device token for the device.
    """
    dmtoken_chars = []
    while len(dmtoken_chars) < 32:
      dmtoken_chars.append(random.choice('0123456789abcdef'))
    dmtoken = ''.join(dmtoken_chars)
    allowed_policy_types = {
      dm.DeviceRegisterRequest.BROWSER: [
          'google/chrome/user',
          'google/chrome/extension'
      ],
      dm.DeviceRegisterRequest.USER: [
          'google/chromeos/user',
          'google/chrome/extension'
      ],
      dm.DeviceRegisterRequest.DEVICE: [
          'google/chromeos/device',
          'google/chromeos/publicaccount',
          'google/chrome/extension',
          'google/chromeos/signinextension'
      ],
      dm.DeviceRegisterRequest.ANDROID_BROWSER: [
          'google/android/user'
      ],
      dm.DeviceRegisterRequest.TT: ['google/chromeos/user',
                                    'google/chrome/user'],
    }
    if machine_id in KIOSK_MACHINE_IDS:
      enrollment_mode = dm.DeviceRegisterResponse.RETAIL
    else:
      enrollment_mode = dm.DeviceRegisterResponse.ENTERPRISE
    self._registered_tokens[dmtoken] = {
      'device_id': device_id,
      'device_token': dmtoken,
      'allowed_policy_types': allowed_policy_types[type],
      'machine_name': 'chromeos-' + machine_id,
      'machine_id': machine_id,
      'enrollment_mode': enrollment_mode,
      'username': username,
    }
    self.WriteClientState()
    return self._registered_tokens[dmtoken]

  def RegisterBrowser(self, dm_token, device_id, machine_name):
    self._registered_tokens[dm_token] = {
      'device_id': device_id,
      'device_token': dm_token,
      'allowed_policy_types': ['google/chrome/machine-level-user',
                               'google/chrome/machine-level-extension'],
      'machine_name': machine_name
    }
    self.WriteClientState()

  def UpdateStateKeys(self, dmtoken, state_keys):
    """Updates the state keys for a given client.

    Args:
      dmtoken: The device management token provided by the client.
      state_keys: The state keys to set.
    """
    if dmtoken in self._registered_tokens:
      self._registered_tokens[dmtoken]['state_keys'] = [
          key.hex() for key in state_keys
      ]
      self.WriteClientState()

  def LookupToken(self, dmtoken):
    """Looks up a device or a user by DM token.

    Args:
      dmtoken: The device management token provided by the client.

    Returns:
      A dictionary with information about a device or user that is registered by
      dmtoken, or None if the token is not found.
    """
    self.ReadClientStateFile()
    return self._registered_tokens.get(dmtoken, None)

  def LookupByStateKey(self, state_key):
    """Looks up a device or a user by a state key.

    Args:
      state_key: The state key provided by the client.

    Returns:
      A dictionary with information about a device or user or None if there is
      no matching record.
    """
    self.ReadClientStateFile()
    for client in list(self._registered_tokens.values()):
      if state_key.hex() in client.get('state_keys', []):
        return client

    return None

  def GetMatchingStateKeyHashes(self, modulus, remainder):
    """Returns all clients registered with the server.

    Returns:
      The list of registered clients.
    """
    self.ReadClientStateFile()
    state_keys = sum([ c.get('state_keys', [])
                       for c in list(self._registered_tokens.values()) ], [])
    hashed_keys = [
        hashlib.sha256(bytes.fromhex(key)).digest() for key in set(state_keys)
    ]
    return [
        hash for hash in hashed_keys
        if int.from_bytes(hash, 'big') % modulus == remainder
    ]

  def GetMatchingSerialHashes(self, modulus, remainder):
    """Returns all serial hashes from configuration.

    Returns:
      The list of hashes
    """
    brand_serial_keys = \
        list(self.GetPolicies().get('initial_enrollment_state', {}).keys())
    hashed_keys = [hashlib.sha256(key).digest()[0:8] for key in
               brand_serial_keys]
    return [
        hash for hash in hashed_keys
        if int.from_bytes(hash, 'big') % modulus == remainder
    ]


  def UnregisterDevice(self, dmtoken):
    """Unregisters a device identified by the given DM token.

    Args:
      dmtoken: The device management token provided by the client.
    """
    if dmtoken in list(self._registered_tokens.keys()):
      del self._registered_tokens[dmtoken]
      self.WriteClientState()

  def ReadClientStateFile(self):
    """ Loads _registered_tokens from client_state_file."""
    if self.client_state_file is None:
      return
    file_timestamp = os.stat(self.client_state_file).st_mtime
    if file_timestamp == self.client_state_file_timestamp:
      return
    logging.info('load client state')
    file_contents = open(self.client_state_file).read()
    self._registered_tokens = json.loads(file_contents, strict=False)
    self.client_state_file_timestamp = file_timestamp

  def WriteClientState(self):
    """Writes the client state back to the file."""
    if self.client_state_file is not None:
      json_data = json.dumps(self._registered_tokens)
      open(self.client_state_file, 'w').write(json_data)
      self.client_state_file_timestamp = os.stat(
                                         self.client_state_file).st_mtime

  def GetBaseFilename(self, policy_selector):
    """Returns the base filename for the given policy_selector.

    Args:
      policy_selector: The policy type and settings entity id, joined by '/'.

    Returns:
      The filename corresponding to the policy_selector, without a file
      extension.
    """
    sanitized_policy_selector = re.sub('[^A-Za-z0-9.@-]', '_', policy_selector)
    return os.path.join(self.data_dir or '',
                        'policy_%s' % sanitized_policy_selector)

  def ListMatchingComponents(self, policy_type):
    """Returns a list of settings entity IDs that have a configuration file.

    Args:
      policy_type: The policy type to look for. Only settings entity IDs for
      file selectors That match this policy_type will be returned.

    Returns:
      A list of settings entity IDs for the given |policy_type| that have a
      configuration file in this server (either as a .bin, .txt or .data file).
    """
    base_name = self.GetBaseFilename(policy_type)
    files = glob.glob('%s_*.*' % base_name)
    len_base_name = len(base_name) + 1
    return [ file[len_base_name:file.rfind('.')] for file in files ]

  def ReadPolicyFromDataDir(self, policy_selector, proto_message):
    """Tries to read policy payload from a file in the data directory.

    First checks for a binary rendition of the policy protobuf in
    <data_dir>/policy_<sanitized_policy_selector>.bin. If that exists, returns
    it. If that file doesn't exist, tries
    <data_dir>/policy_<sanitized_policy_selector>.txt and decodes that as a
    protobuf using proto_message. If that fails as well, returns None.

    Args:
      policy_selector: Selects which policy to read.
      proto_message: Optional protobuf message object used for decoding the
          proto text format.

    Returns:
      The binary payload message, or None if not found.
    """
    base_filename = self.GetBaseFilename(policy_selector)

    # Try the binary payload file first.
    try:
      return open(base_filename + '.bin', 'rb').read()
    except IOError:
      pass

    # If that fails, try the text version instead.
    if proto_message is None:
      return None

    try:
      text = open(base_filename + '.txt').read()
      google.protobuf.text_format.Merge(text, proto_message)
      return proto_message.SerializeToString()
    except IOError:
      return None
    except google.protobuf.text_format.ParseError:
      return None

  def ReadPolicyDataFromDataDir(self, policy_selector):
    """Returns the external policy data for |policy_selector| if found.

    Args:
      policy_selector: Selects which policy to read.

    Returns:
      The data for the corresponding policy type and entity id, if found.
    """
    base_filename = self.GetBaseFilename(policy_selector)
    try:
      return open(base_filename + '.data', 'rb').read()
    except IOError:
      return None

  def GetBaseURL(self):
    """Returns the server base URL.

    Respects the |server_base_url| configuration parameter, if present. Falls
    back to construct the URL from the server hostname and port otherwise.

    Returns:
      The URL to use for constructing URLs that get returned to clients.
    """
    base_url = self.server_base_url
    if base_url is None:
      base_url = 'http://%s:%s' % self.server_address[:2]

    return base_url


class PolicyServerRunner(testserver_base.TestServerRunner):

  def __init__(self):
    super(PolicyServerRunner, self).__init__()

  def create_server(self, server_data):
    data_dir = self.options.data_dir or ''
    config_file = (self.options.config_file or
                   os.path.join(data_dir, 'device_management'))
    server = PolicyTestServer((self.options.host, self.options.port),
                              data_dir, config_file,
                              self.options.client_state_file,
                              self.options.policy_keys,
                              self.options.rotate_keys_automatically,
                              self.options.server_base_url)
    server_data['port'] = server.server_port
    return server

  def add_options(self):
    testserver_base.TestServerRunner.add_options(self)
    self.option_parser.add_option('--client-state', dest='client_state_file',
                                  help='File that client state should be '
                                  'persisted to. This allows the server to be '
                                  'seeded by a list of pre-registered clients '
                                  'and restarts without abandoning registered '
                                  'clients.')
    self.option_parser.add_option('--policy-key', action='append',
                                  dest='policy_keys',
                                  help='Specify a path to a PEM-encoded '
                                  'private key to use for policy signing. May '
                                  'be specified multiple times in order to '
                                  'load multiple keys into the server. The '
                                  'server will use a canned key if none is '
                                  'specified on the command line. The test '
                                  'server will also look for a verification '
                                  'signature file in the same location: '
                                  '<filename>.sig and if present will add the '
                                  'signature to the policy blob as appropriate '
                                  'via the '
                             'new_public_key_verification_signature_deprecated '
                                  'field.')
    self.option_parser.add_option('--rotate-policy-keys-automatically',
                                  action='store_true',
                                  dest='rotate_keys_automatically',
                                  help='If present, then the policy keys will '
                                  'be rotated in a round-robin fashion for '
                                  'each policy request (by default, either the '
                                  'key specified in the config or the first '
                                  'key will be used for all requests).')
    self.option_parser.add_option('--log-level', dest='log_level',
                                  default='WARN',
                                  help='Log level threshold to use.')
    self.option_parser.add_option('--config-file', dest='config_file',
                                  help='Specify a configuration file to use '
                                  'instead of the default '
                                  '<data_dir>/device_management')
    self.option_parser.add_option('--server-base-url', dest='server_base_url',
                                  help='The server base URL to use when '
                                  'constructing URLs to return to the client.')

  def run_server(self):
    logger = logging.getLogger()
    logger.setLevel(getattr(logging, str(self.options.log_level).upper()))
    if (self.options.log_to_console):
      logger.addHandler(logging.StreamHandler())
    if (self.options.log_file):
      logger.addHandler(logging.FileHandler(self.options.log_file))

    testserver_base.TestServerRunner.run_server(self)


if __name__ == '__main__':
  sys.exit(PolicyServerRunner().main())

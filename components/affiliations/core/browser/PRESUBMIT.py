import sys

# Optional but recommended
PRESUBMIT_VERSION = '2.0.0'

# Mandatory: run under Python 3
USE_PYTHON3 = True


def _ImportPresubmitSupport(input_api):
  old_sys_path = sys.path[:]
  try:
    cwd = input_api.PresubmitLocalPath()
    sys.path += [input_api.os_path.join(cwd, '..', '..', '..')]
    from password_manager import presubmit_support
  finally:
    sys.path = old_sys_path
  return presubmit_support


def CheckServerProtoChangeAck(input_api, output_api):
  presubmit_support = _ImportPresubmitSupport(input_api)
  return presubmit_support.CheckServerProtoChangeAck(
      input_api,
      output_api,
      'affiliation_api.proto',
      'http://google3/identity/affiliation/proto/affiliation_service.proto')

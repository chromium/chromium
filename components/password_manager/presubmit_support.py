# Mandatory: run under Python 3
USE_PYTHON3 = True


def CheckServerProtoChangeAck(input_api, output_api, file_name, server_proto_url):
  skip_check = 'SERVER_PROTO_CHANGE_FIRST_ACK' in input_api.change.tags

  if skip_check:
    return []

  for f in input_api.AffectedFiles():
    if f.LocalPath().endswith(file_name):
      return [
          output_api.PresubmitError(
              f'''Please make sure that the server proto updated first!
              1. Update {server_proto_url} first
              2. Wait for the server proto change to be deployed rollback-safe
              3. Add the tag SERVER_PROTO_CHANGE_FIRST_ACK to the CL description
              4. Submit the CL'''
          )
      ]
  return []

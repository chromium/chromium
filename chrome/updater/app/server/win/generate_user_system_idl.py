# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A tool for generating IDL files from a template, with distinct user and
system identities for interfaces that are decorated with `BEGIN_INTERFACE` and
`END_INTERFACE`.

`BEGIN_INTERFACE` takes the placeholder guid, the interface that needs distinct
identities, as well as any other items that need to be distinct for `user` and
`system` respectively.

Here is an example:

```
BEGIN_INTERFACE(
  {
    "user": {
      "PLACEHOLDER-GUID-2FCD14AF-B645-4351-8359-E80A0E202A0B":
          "PLACEHOLDER-GUID-9AD1A645-5A4B-4D36-BC21-F0059482E6EA",
      "ICompleteStatus":"ICompleteStatusUser"
    },
    "system": {
      "PLACEHOLDER-GUID-2FCD14AF-B645-4351-8359-E80A0E202A0B":
          "PLACEHOLDER-GUID-E2BD9A6B-0A19-4C89-AE8B-B7E9E51D9A07",
      "ICompleteStatus":"ICompleteStatusSystem"
    }
  }
)
[
  uuid(PLACEHOLDER-GUID-2FCD14AF-B645-4351-8359-E80A0E202A0B),
  oleautomation,
  pointer_default(unique)
]
interface ICompleteStatus : IUnknown {
  [propget] HRESULT statusCode([out, retval] LONG*);
  [propget] HRESULT statusMessage([out, retval] BSTR*);
};
END_INTERFACE
```

The example input above will produce the following output in the IDL:

```
[
  uuid(PLACEHOLDER-GUID-2FCD14AF-B645-4351-8359-E80A0E202A0B),
  oleautomation,
  pointer_default(unique)
]
interface ICompleteStatus : IUnknown {
  [propget] HRESULT statusCode([out, retval] LONG*);
  [propget] HRESULT statusMessage([out, retval] BSTR*);
};
[
  uuid(PLACEHOLDER-GUID-9AD1A645-5A4B-4D36-BC21-F0059482E6EA),
  oleautomation,
  pointer_default(unique)
]
interface ICompleteStatusUser : IUnknown {
  [propget] HRESULT statusCode([out, retval] LONG*);
  [propget] HRESULT statusMessage([out, retval] BSTR*);
};
[
  uuid(PLACEHOLDER-GUID-E2BD9A6B-0A19-4C89-AE8B-B7E9E51D9A07),
  oleautomation,
  pointer_default(unique)
]
interface ICompleteStatusSystem : IUnknown {
  [propget] HRESULT statusCode([out, retval] LONG*);
  [propget] HRESULT statusMessage([out, retval] BSTR*);
};
```

Usage:
    python3 generate_user_system_idl.py --idl_template_file updater_idl.template
        --idl_output_file updater_idl_new.idl
"""

import getopt
import json
import sys


def _GetDict(f_in):
    dict_text = ''
    while True:
        line = f_in.readline()
        if not line:
            break

        if line.startswith(')'):
            break
        dict_text += line

    return json.loads(dict_text)


def _GetInterface(f_in):
    interface_text = ''
    while True:
        line = f_in.readline()
        if not line:
            break

        if line.startswith('END_INTERFACE'):
            break
        interface_text += line

    return interface_text


def _GenerateIDLFile(idl_template_filename, idl_output_filename):
    idl_output = ''
    f_in = open(idl_template_filename, 'r')

    while True:
        line = f_in.readline()
        if not line:
            break

        if line.startswith('BEGIN_INTERFACE('):
            dict = _GetDict(f_in)
            interface = _GetInterface(f_in)
            idl_output += interface

            for user_or_system in ['user', 'system']:
                interface_gen = interface
                for key, value in dict[user_or_system].items():
                    interface_gen = interface_gen.replace(key, value)
                idl_output += interface_gen
        else:
            idl_output += line

    f_in.close()
    with open(idl_output_filename, 'w') as f_out:
        f_out.write('// *** AUTO-GENERATED IDL FILE. ***\n\n')
        f_out.write(idl_output)


def _Usage():
    """Prints out script usage information."""
    print("""
Usage:
  generate_user_system_idl.py [--help
                               | --idl_template_file filename
                                 --idl_output_file filename]

Options:
  --help                        Show this information.
  --idl_output_file filename    Path/name of output IDL filename.
  --idl_template_file filename  Path/name of input IDL template.
""")


def _Main():
    """Generates IDL files from a template for user and system marshaling."""
    argument_list = ['help', 'idl_template_file=', 'idl_output_file=']
    (opts, unused_args) = getopt.getopt(sys.argv[1:], '', argument_list)
    if not opts or ('--help', '') in opts:
        _Usage()
        sys.exit()

    idl_template_filename = ''
    idl_output_filename = ''

    for (o, v) in opts:
        if o == '--idl_template_file':
            idl_template_filename = v
        if o == '--idl_output_file':
            idl_output_filename = v

    if not idl_template_filename:
        raise StandardError('no idl_template_filename specified')
    if not idl_output_filename:
        raise StandardError('no idl_output_filename specified')

    _GenerateIDLFile(idl_template_filename, idl_output_filename)
    sys.exit()


if __name__ == '__main__':
    _Main()

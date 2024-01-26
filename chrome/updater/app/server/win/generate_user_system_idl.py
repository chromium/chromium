# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A tool for generating IDL files from a template, with distinct user and
system identities for interfaces that are decorated with `BEGIN_INTERFACE` and
`END_INTERFACE`.

`BEGIN_INTERFACE` takes the placeholder guid, the interface that needs distinct
identities, as well as any other items that need to be distinct for `user` and
`system` respectively.

`addToLibrary` specifies the interfaces that should be listed in the `library`
section of the generated IDL. For instance:
* `"addToLibrary": ["user", "system"]` will add the `IInterfaceUser` and
the `IInterfaceSystem` to the `library` section.
* `"addToLibrary": ["", "user", "system"]` will add `IInterface`,
`IInterfaceUser` and `IInterfaceSystem` to the `library` section.
* `"addToLibrary": ["user"]` will add only `IInterfaceUser` to the
`library` section.

`includeFor` is an optional qualifier that indicates what scope the interface
and the specializations should be included for, when used in conjunction with
the optional `--generate_for` command line option. `includeFor` is ignored if
no `--generate_for` command line option is specified. For instance:
* `"includeFor": ["system"]` will add the interface and the specializations only
when using the `--generate_for=system` command line option, but not when using
the `--generate_for=user` command line option.

Here is an example:

```
BEGIN_INTERFACE(
  {
    "uuid": {
      "user":"PLACEHOLDER-GUID-9AD1A645-5A4B-4D36-BC21-F0059482E6EA",
      "system":"PLACEHOLDER-GUID-E2BD9A6B-0A19-4C89-AE8B-B7E9E51D9A07"
    },
    "tokensToSuffix": ["ICompleteStatus"],
    "addToLibrary": ["", "user", "system"]
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

[
...
]
library UpdaterLib {
  INTERFACES_IN_LIBRARY;
};
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

[
...
]
library UpdaterLib {
  interface ICompleteStatus;
  interface ICompleteStatusUser;
  interface ICompleteStatusSystem;
};
```

Usage:
    python3 generate_user_system_idl.py --idl_template_file updater_idl.template
        --idl_output_file updater_idl_new.idl
"""

import argparse
import json
import re


def _GenerateIDLFile(idl_template_filename, idl_output_filename, generate_for):
    pattern = re.compile(
        r'''BEGIN_INTERFACE\(
            (.*?)    # Group for the replacement dictionary.
            \)
            (.*?)    # Group for interface text.
            END_INTERFACE''', re.DOTALL | re.X)

    with open(idl_template_filename, 'rt') as f:
        matches = re.split(pattern, f.read())

        # Copy anything before the first 'BEGIN_INTERFACE' to output.
        idl_output = [matches[0]]
        interfaces_in_library = []

        for i in range(1, len(matches), 3):
            replacement_dict = json.loads(matches[i])
            interface_text = matches[i + 1]
            trailer = matches[i + 2]
            interface_base_name = re.search(r'interface (\w+) :',
                                            interface_text).group(1)
            if not generate_for or generate_for in replacement_dict.get(
                    'includeFor',
                {}) or generate_for in replacement_dict['addToLibrary']:
                idl_output.append(interface_text)
                if "" in replacement_dict['addToLibrary']:
                    interfaces_in_library.append("interface " +
                                                 interface_base_name)

                for scope, placeholder_guid in replacement_dict.get(
                        'uuid', {}).items():
                    if generate_for and generate_for != scope:
                        continue
                    interfaces_in_library.append("interface " +
                                                 interface_base_name +
                                                 scope.title())
                    interface_gen = re.sub(r'(uuid\().*?(\))',
                                           r'\1%s\2' % placeholder_guid,
                                           interface_text)
                    for k in replacement_dict['tokensToSuffix']:
                        interface_gen = re.sub(r'\b%s\b' % k,
                                               k + scope.title(),
                                               interface_gen)
                    idl_output.append(interface_gen)

            if trailer.strip():
                trailer = re.sub(r'INTERFACES_IN_LIBRARY',
                                 ';\n  '.join(interfaces_in_library), trailer)
                idl_output.append(trailer)

        with open(idl_output_filename, 'w') as f_out:
            f_out.write('// *** AUTO-GENERATED IDL FILE. ***\n\n')
            for item in idl_output:
                f_out.write(item)


def _Main():
    """Generates IDL files from a template for user and system marshaling."""
    cmd_parser = argparse.ArgumentParser(
        description='Tool to generate IDL from template.')

    cmd_parser.add_argument('--idl_template_file',
                            dest='idl_template_file',
                            type=str,
                            required=True,
                            help='Input IDL template file.')
    cmd_parser.add_argument('--idl_output_file',
                            type=str,
                            required=True,
                            help='Output IDL file.')
    cmd_parser.add_argument('--generate_for',
                            type=str,
                            help='Generate the IDL for `user` or `system`.')
    flags = cmd_parser.parse_args()

    _GenerateIDLFile(flags.idl_template_file, flags.idl_output_file,
                     flags.generate_for)


if __name__ == '__main__':
    _Main()

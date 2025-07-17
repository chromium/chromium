#!/usr/bin/env python3
#
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script generates a Domato grammar from the root interfaces json file.
To do that, it parses the database and builds the javascript boilerplate so
that Domato can interact with the renderer exposed browser interfaces.

Usage: ./generator.py
            -p /path/to/interfaces.json -r /path/to/root_out_dir -o outfile.txt
"""

from __future__ import annotations

import argparse
import os
import sys
from typing import List, Dict, Union
import dataclasses
import json
from collections import defaultdict


def _GetDirAbove(dirname: str):
    """Returns the directory "above" this file containing |dirname| (which must
  also be "above" this file)."""
    path = os.path.abspath(__file__)
    while True:
        path, tail = os.path.split(path)
        if not tail:
            return None
        if tail == dirname:
            return path


SOURCE_DIR = _GetDirAbove('chrome')

sys.path.append(os.path.join(SOURCE_DIR, 'build'))
sys.path.append(os.path.join(SOURCE_DIR, 'mojo/public/tools/mojom/'))
sys.path.append(
    os.path.join(SOURCE_DIR, 'mojo/public/tools/bindings/generators'))

import action_helpers
from mojom.generate import module as mojom
from mojom_js_generator import JavaScriptStylizer


@dataclasses.dataclass
class DomatoType:
    name: str
    should_record: bool = False


@dataclasses.dataclass
class Rule:
    lhs: DomatoType
    rhs: List[Union[str, DomatoType]]


@dataclasses.dataclass
class DomatoGrammarBuilder:

    def __init__(self):
        self.rules: List[Rule] = []
        self.lines: List[Rule] = []
        self.helperlines: List[Rule] = []
        self.context_interfaces = set()
        self.process_interfaces = set()

    def add_rule(self, rule: Rule):
        self.rules.append(rule)

    def add_line(self, rule: Rule):
        self.lines.append(rule)

    def add_helper_line(self, rule: Rule):
        self.helperlines.append(rule)


MOJO_KIND_TO_DOMATO_TYPE = {
    mojom.BOOL: DomatoType(name='Boolean'),
    mojom.INT8: DomatoType(name='int8'),
    mojom.INT16: DomatoType(name='int16'),
    mojom.INT32: DomatoType(name='int32'),
    mojom.INT64: DomatoType(name='int64'),
    mojom.UINT8: DomatoType(name='uint8'),
    mojom.UINT16: DomatoType(name='uint16'),
    mojom.UINT32: DomatoType(name='uint32'),
    mojom.UINT64: DomatoType(name='uint64'),
    mojom.FLOAT: DomatoType(name='float'),
    mojom.DOUBLE: DomatoType(name='double'),
    mojom.STRING: DomatoType(name='String')
}

DEFINED_TYPES = set()


def adapt_type_name(name: str):
    return name.replace('.', '_')


def build_stub_type_rules(kind, builder: DomatoGrammarBuilder):
    """To build an interface stub rule
         1. Build the rule for the stub class
         2. Add the rule to instantiate the stub class
       This resulting rules look as follows:
         <new Stub_Class_Rule> = class {...};
         <new Stub_Rule> = new <Stub_Class_Rule>();"""
    stub_type = DomatoType(name=f'{adapt_type_name(kind.qualified_name)}Stub',
                           should_record=True)
    if stub_type.name in DEFINED_TYPES:
        return stub_type

    DEFINED_TYPES.add(stub_type.name)

    stub_type_class = DomatoType(
        name=f'{adapt_type_name(kind.qualified_name)}StubClass',
        should_record=True)

    # Generates the rule to create an interface stub class.
    # Example:
    # <InterfaceStubClass> = class {
    #   method1(args) {}
    #   method2(){return <response_rule>;}
    #   method3(){}
    # };
    rhs = ['class {']
    method_definitions = []
    for method in kind.methods:
        method_definition = [method.name, '(']
        parameters = []
        for parameter in method.parameters:
            parameters.append(parameter.name)
            parameters.append(',')
        if parameters:
            parameters.pop()
        method_definition += parameters
        method_definition.append('){')
        if method.response_parameters is not None:
            method_definition.append('return Promise.resolve(')
            response_types = []
            for response_param in method.response_parameters:
                response_types.append(f'{response_param.name}:')
                response_types.append(
                    build_kind_rules(response_param.kind, builder))
                response_types.append(',')
            if response_types:
                response_types.pop()
                method_definition.append('{')
                method_definition += response_types
                method_definition.append('}')

            method_definition.append(');')

        method_definition.append('}')
        method_definitions += method_definition

    rhs += method_definitions
    rhs += ['};']

    builder.add_helper_line(Rule(stub_type_class, rhs))
    builder.add_helper_line(Rule(stub_type, ['new ', stub_type_class, '();']))
    return stub_type


def build_receiver_type_rules(kind, builder: DomatoGrammarBuilder):
    """Generates the following helper line:
        <new InterfaceReceiverRule> = new InterfaceReceiver(<Stub_Rule>);
    """
    receiver_type = DomatoType(
        name=f'{adapt_type_name(kind.kind.qualified_name)}Receiver',
        should_record=True)
    if receiver_type.name in DEFINED_TYPES:
        return receiver_type

    DEFINED_TYPES.add(receiver_type.name)
    stub_type = build_stub_type_rules(kind.kind, builder)
    builder.add_helper_line(
        Rule(receiver_type, [
            'new ', f'{kind.kind.module.namespace}.{kind.kind.name}Receiver(',
            stub_type, ');'
        ]))

    return receiver_type


def build_pending_remote_rules(kind, builder: DomatoGrammarBuilder):
    pending_remote_type = DomatoType(
        name=f'{adapt_type_name(kind.kind.qualified_name)}PendingRemote',
        should_record=False)
    if pending_remote_type.name in DEFINED_TYPES:
        return pending_remote_type

    DEFINED_TYPES.add(pending_remote_type.name)

    # To send a pending remote as an argument,
    # we need to:
    #  1. Create a receiver (which involves creating an interface stub)
    #  2. Create a pending_remote using the created receiver
    receiver_type = build_receiver_type_rules(kind, builder)

    builder.add_rule(
        Rule(pending_remote_type,
             [receiver_type, '.$.bindNewPipeAndPassRemote()']))

    return pending_remote_type


def build_pending_associated_remote_rules(kind, builder: DomatoGrammarBuilder):
    pending_associated_remote_type = DomatoType(
        name=
        f'{adapt_type_name(kind.kind.qualified_name)}PendingAssociatedRemote',
        should_record=False)

    if pending_associated_remote_type.name in DEFINED_TYPES:
        return pending_associated_remote_type

    # To send a pending associated remote as an argument,
    # we need to:
    #  1. Create a receiver (which involves creating an interface stub)
    #  2. Create a pending_associated_remote using the created receiver
    receiver_type = build_receiver_type_rules(kind, builder)

    builder.add_rule(
        Rule(pending_associated_remote_type,
             [receiver_type, '.$.associateAndPassRemote()']))

    return pending_associated_remote_type


def build_pending_receiver_rules(kind, builder: DomatoGrammarBuilder):
    pending_receiver_type = DomatoType(
        name=f'{adapt_type_name(kind.kind.qualified_name)}PendingReceiver')
    if pending_receiver_type.name in DEFINED_TYPES:
        return pending_receiver_type

    DEFINED_TYPES.add(pending_receiver_type.name)
    remote_type = DomatoType(
        name=f'{adapt_type_name(kind.kind.qualified_name)}Remote',
        should_record=True)

    if not remote_type.name in DEFINED_TYPES:
        builder.add_helper_line(
            Rule(remote_type, [
                f'new {kind.kind.module.namespace}.{kind.kind.name}Remote();'
            ]))

    DEFINED_TYPES.add(remote_type.name)

    builder.add_rule(
        Rule(pending_receiver_type,
             [remote_type, '.$.bindNewPipeAndPassReceiver()']))
    return pending_receiver_type


def build_pending_associated_receiver_rules(kind,
                                            builder: DomatoGrammarBuilder):
    pending_associated_receiver_type = DomatoType(
        name=
        f'{adapt_type_name(kind.kind.qualified_name)}PendingAssociatedReceiver'
    )
    if pending_associated_receiver_type.name in DEFINED_TYPES:
        return pending_associated_receiver_type

    DEFINED_TYPES.add(pending_associated_receiver_type.name)
    remote_type = DomatoType(
        name=f'{adapt_type_name(kind.kind.qualified_name)}Remote',
        should_record=True)
    if not remote_type.name in DEFINED_TYPES:
        builder.add_helper_line(
            Rule(remote_type, [
                f'new {kind.kind.module.namespace}.{kind.kind.name}Remote();'
            ]))

    DEFINED_TYPES.add(remote_type.name)

    builder.add_rule(
        Rule(pending_associated_receiver_type,
             [remote_type, '.$.associateAndPassReceiver()']))
    return pending_associated_receiver_type


def build_array_rules(kind, builder: DomatoGrammarBuilder):
    element_type = build_kind_rules(kind.kind, builder)
    array_type = DomatoType(name=f'{element_type.name}Array')
    if array_type.name in DEFINED_TYPES:
        return array_type
    DEFINED_TYPES.add(array_type.name)

    array_elements_type = DomatoType(name=f'{element_type.name}ArrayElements')
    builder.add_rule(Rule(array_elements_type, []))

    builder.add_rule(
        Rule(array_elements_type, [element_type, ',', array_elements_type]))

    builder.add_rule(Rule(array_type, ['[', array_elements_type, ']']))
    # TODO: Handle fixed-size arrays
    return array_type


def build_struct_rules(kind, builder: DomatoGrammarBuilder):
    struct_type = DomatoType(name=adapt_type_name(kind.qualified_name),
                             should_record=True)
    if struct_type.name in DEFINED_TYPES:
        return struct_type
    DEFINED_TYPES.add(struct_type.name)

    fields = []
    # fields are empty for native_only structs
    for field in kind.fields:
        field_type = build_kind_rules(field.kind, builder)
        fields += [field.name, ':', field_type, ',']
    if fields:
        fields.pop()
    rhs = ['{'] + fields + ['};']
    builder.add_helper_line(Rule(struct_type, rhs))

    return struct_type


def build_enum_rules(kind, builder: DomatoGrammarBuilder):
    enum_type = DomatoType(adapt_type_name(kind.qualified_name))
    if enum_type.name in DEFINED_TYPES:
        return enum_type
    DEFINED_TYPES.add(enum_type.name)

    for field in kind.fields:
        builder.add_rule(Rule(enum_type, [str(field.numeric_value)]))

    # We don't have any information about native_only enums.
    # Setting zero as a possible value
    if kind.native_only:
        builder.add_rule(Rule(enum_type, ['0']))

    return enum_type


def build_union_rules(kind, builder: DomatoGrammarBuilder):
    union_type = DomatoType(adapt_type_name(kind.qualified_name))
    if union_type.name in DEFINED_TYPES:
        return union_type
    DEFINED_TYPES.add(union_type.name)

    for field in kind.fields:
        # For each field, we create the following rule:
        # <union_type> = {field_name: <field_type>};
        field_type = build_kind_rules(field.kind, builder)
        rhs = ['{', f'{field.name}:', field_type, '}']
        builder.add_rule(Rule(union_type, rhs))

    return union_type


def build_nullable_kind_rules(kind, builder: DomatoGrammarBuilder):
    """Generates the rules to build a nullable kind:
            <nullable_kind> = null
            <nullable_kind> = <kind>
    """
    assert (mojom.IsNullableKind(kind))
    un_nullable = kind.MakeUnnullableKind()
    type = build_kind_rules(un_nullable, builder)

    nullable_type = DomatoType(type)
    nullable_type.name = f'Nullable_{type.name}'

    if nullable_type.name in DEFINED_TYPES:
        return nullable_type
    DEFINED_TYPES.add(nullable_type.name)

    builder.add_rule(Rule(nullable_type, ['null']))
    builder.add_rule(Rule(nullable_type, [type]))

    return nullable_type


def build_map_rules(kind, builder: DomatoGrammarBuilder):
    key_type = build_kind_rules(kind.key_kind, builder)
    value_type = build_kind_rules(kind.value_kind, builder)

    map_type = DomatoType(name=f'Map_{key_type.name}_{value_type.name}',
                          should_record=True)
    if map_type.name in DEFINED_TYPES:
        return map_type
    DEFINED_TYPES.add(map_type.name)

    map_elements = DomatoType(
        name=f'Map_{key_type.name}_{value_type.name}Elements')

    # <map_elements> =
    # <map_elements> = <key>:<value>, <map_elements>
    # <new map> = {<map_elements>}

    builder.add_rule(Rule(map_elements, []))
    builder.add_rule(
        Rule(map_elements, [key_type, ':', value_type, ',', map_elements]))
    builder.add_helper_line(Rule(map_type, ['{', map_elements, '};']))

    return map_type


def build_handle_rules(kind, builder: DomatoGrammarBuilder):
    handle_type = DomatoType(name='mojo_handle')
    if handle_type.name in DEFINED_TYPES:
        return handle_type

    DEFINED_TYPES.add(handle_type.name)
    # Send an invalid handle.
    builder.add_rule(Rule(handle_type, "{value: 0}"))
    return handle_type


def build_kind_rules(kind, builder):
    if kind in MOJO_KIND_TO_DOMATO_TYPE:
        return MOJO_KIND_TO_DOMATO_TYPE[kind]

    if mojom.IsNullableKind(kind):
        return build_nullable_kind_rules(kind, builder)

    if mojom.IsPendingRemoteKind(kind):
        return build_pending_remote_rules(kind, builder)

    if mojom.IsPendingReceiverKind(kind):
        return build_pending_receiver_rules(kind, builder)

    if mojom.IsArrayKind(kind):
        return build_array_rules(kind, builder)

    if mojom.IsStructKind(kind):
        return build_struct_rules(kind, builder)

    if mojom.IsEnumKind(kind):
        return build_enum_rules(kind, builder)

    if mojom.IsUnionKind(kind):
        return build_union_rules(kind, builder)

    if mojom.IsPendingAssociatedRemoteKind(kind):
        return build_pending_associated_remote_rules(kind, builder)

    if mojom.IsPendingAssociatedReceiverKind(kind):
        return build_pending_associated_receiver_rules(kind, builder)

    if mojom.IsMapKind(kind):
        return build_map_rules(kind, builder)

    if mojom.IsAnyHandleKind(kind):
        return build_handle_rules(kind, builder)

    return DomatoType('UnHandledType')


def get_response_type(kind):
    if kind in MOJO_KIND_TO_DOMATO_TYPE:
        return MOJO_KIND_TO_DOMATO_TYPE[kind]

    if mojom.IsNullableKind(kind):
        un_nullable = kind.MakeUnnullableKind()
        type = get_response_type(un_nullable)

        nullable_type = DomatoType(type)
        nullable_type.name = f'Nullable{type.name}'
        return nullable_type

    if mojom.IsPendingRemoteKind(kind):
        return DomatoType(
            name=f'{adapt_type_name(kind.kind.qualified_name)}PendingRemote',
            should_record=True)

    if mojom.IsPendingReceiverKind(kind):
        return DomatoType(
            name=f'{adapt_type_name(kind.kind.qualified_name)}PendingReceiver',
            should_record=True)

    if mojom.IsArrayKind(kind):
        element_type = get_response_type(kind.kind)
        return DomatoType(name=f'{element_type.name}Array')

    if mojom.IsStructKind(kind):
        return DomatoType(name=f'{adapt_type_name(kind.qualified_name)}')

    if mojom.IsEnumKind(kind):
        return DomatoType(adapt_type_name(kind.qualified_name))

    if mojom.IsUnionKind(kind):
        return DomatoType(adapt_type_name(kind.qualified_name))

    if mojom.IsPendingAssociatedRemoteKind(kind):
        return DomatoType(
            name=
            f'{adapt_type_name(kind.kind.qualified_name)}PendingAssociatedRemote',
            should_record=False)

    if mojom.IsPendingAssociatedReceiverKind(kind):
        return DomatoType(
            name=
            f'{adapt_type_name(kind.kind.qualified_name)}PendingAssociatedReceiver',
            should_record=False)

    if mojom.IsMapKind(kind):
        key_type = get_response_type(kind.key_kind)
        value_type = get_response_type(kind.value_kind)
        map_type = DomatoType(name=f'Map_{key_type.name}_{value_type.name}',
                              should_record=True)
        return map_type

    if mojom.IsAnyHandleKind(kind):
        return DomatoType(name='mojo_handle')

    return DomatoType('UnHandledType')


def build_method_rules(interface_type, method, builder: DomatoGrammarBuilder):
    rule = ['await ']
    rule += [interface_type, f'.{method.name}(']
    should_skip = False
    param_kinds = []
    # TODO: Support methods that take a handle as a parameter.
    if any(
            mojom.IsAnyHandleKind(parameter.kind)
            for parameter in method.parameters):
        return
    for parameter in method.parameters:
        param_kinds.append(build_kind_rules(parameter.kind, builder))
        param_kinds.append(',')

    if param_kinds:
        param_kinds.pop()

    rule += param_kinds
    rule += [');']

    lhs = DomatoType('', False)
    if method.response_parameters:
        lhs.name = f'{adapt_type_name(method.interface.qualified_name)}_{method.mojom_name}ResponseType'
        lhs.should_record = True
        for response_param in method.response_parameters:
            # <param_type> = <method_return_type>.param_name
            if response_param.kind in MOJO_KIND_TO_DOMATO_TYPE:
                # Don't reuse basic types returned from functions
                continue
            param_type = get_response_type(response_param.kind)
            builder.add_rule(Rule(param_type, [lhs, '.', response_param.name]))

    if should_skip:
        return
    builder.add_line(Rule(lhs, rule))
    return


def get_interface(interface_module: mojom.Module,
                  interface_name: str) -> mojom.Interface:
    for interface in interface_module.interfaces:
        if interface.qualified_name == interface_name:
            return interface
    raise f'Interface {interface_name} not found in module {interface_module.name}'


def build_remote_type_rules(interface, builder: DomatoGrammarBuilder):
    remote_type = DomatoType(
        f'{adapt_type_name(interface.qualified_name)}Remote', True)
    if remote_type.name in DEFINED_TYPES:
        return remote_type

    DEFINED_TYPES.add(remote_type.name)
    scope = '"context"'
    if interface.qualified_name in builder.process_interfaces:
        scope = '"process"'
    rhs = [
        'root_interface_binder(',
        f'{interface.module.namespace}.{interface.name}Remote', ',', scope,
        ');'
    ]
    builder.add_helper_line(Rule(remote_type, rhs))
    return remote_type


def build_root_interface_rules(root_interface_name, interface_module, builder):
    root_interface = get_interface(interface_module, root_interface_name)
    interface_type = build_remote_type_rules(root_interface, builder)
    for method in root_interface.methods:
        build_method_rules(interface_type, method, builder)
    return


UNFOUND_FILES = set()


def collect_module_dependencies(m: mojom.Module, VISITED_NODES: Dict[str, str],
                                INTERFACE_JS_MODULES: List, root_build_dir):
    js_path = f'gen/{m.path}-lite.js'

    full_path = os.path.join(root_build_dir, js_path)
    if not os.path.isfile(full_path):
        UNFOUND_FILES.add(full_path)
        return False

    if VISITED_NODES[js_path] == 'visited':
        return True

    if VISITED_NODES[js_path] == 'visiting':
        return False

    VISITED_NODES[js_path] = 'visiting'

    for imported_module in m.imports:
        res = collect_module_dependencies(imported_module, VISITED_NODES,
                                          INTERFACE_JS_MODULES, root_build_dir)
        if not res:
            return False

    VISITED_NODES[js_path] = 'visited'
    INTERFACE_JS_MODULES.append(js_path)
    return True


def main():
    parser = argparse.ArgumentParser(
        description='Generates a grammar for Domato that describes WebIDLs.')
    parser.add_argument('-p',
                        '--path',
                        required=True,
                        help="Path to the root interfaces json file.")
    parser.add_argument('-o',
                        '--outfile',
                        required=True,
                        help="Path to the output grammar.")
    parser.add_argument('-r',
                        '--root-build-dir',
                        required=True,
                        help="The path to the root out dir.")
    parser.add_argument(
        '-i',
        '--interface-name',
        required=False,
        help=
        'If provided, this generates the rules for the specified interface and its dependencies only.'
    )

    args = parser.parse_args()
    interfaces = {}
    builder = DomatoGrammarBuilder()
    # Add basic rules
    builder.add_rule(Rule(DomatoType('String'), ['"<string>"']))
    builder.add_rule(Rule(DomatoType('Boolean'), ['0']))
    builder.add_rule(Rule(DomatoType('Boolean'), ['1']))
    INTERFACE_JS_MODULES = []
    VISITED_NODES = defaultdict(str)
    with open(args.path, 'r') as f:
        data = json.load(f)
        for entry in data['context_interfaces']:
            # entry[1] is the fully qualified interface name
            # entry[0] is the path to the interface mojom file
            interfaces[entry[1]] = entry[0]
            builder.context_interfaces.add(entry[1])
        for entry in data['process_interfaces']:
            # entry[1] is the fully qualified interface name
            # entry[0] is the path to the interface mojom file
            interfaces[entry[1]] = entry[0]
            builder.process_interfaces.add(entry[1])

        # maps the fully qualified root interface name to the mojom-module file path
        interface_modules = {}
        for entry in data['interfaces']:
            # entry[1] is the fully qualified interface name
            if entry[1] in interfaces:
                # entry[0] is the path to the mojom-module file
                interface_modules[entry[1]] = entry[0]

        for root_interface_name, module_path in interface_modules.items():
            if args.interface_name and args.interface_name not in root_interface_name:
                continue
            module_path = os.path.join(args.root_build_dir, module_path)
            with open(module_path, 'rb') as f:
                m = mojom.Module.Load(f)
                result = collect_module_dependencies(m, VISITED_NODES,
                                                     INTERFACE_JS_MODULES,
                                                     args.root_build_dir)
                # Only build rules for modules that have a corresponding MojoJS binding
                # for it and all its dependencies.
                if result:
                    m.Stylize(JavaScriptStylizer())
                    build_root_interface_rules(root_interface_name, m, builder)

    html_file_name = f'{args.root_build_dir}/mojo_js_in_process_fuzzer.html'

    if args.interface_name:
        html_file_name = f'{args.root_build_dir}/{args.interface_name}_in_process_fuzzer.html'

    with action_helpers.atomic_output(html_file_name, mode="w") as f:
        f.write('<html>\n')
        f.write(' <body>\n')
        f.write(
            '  <script src="/gen/mojo/public/js/mojo_bindings_lite.js"></script>\n'
        )
        for js_module in INTERFACE_JS_MODULES:
            f.write(f'  <script src="/{js_module}"></script>\n')
        f.write('  <script>\n')
        f.write(
            '   function root_interface_binder(root_interface_class, scope) {\n'
        )
        f.write('      root_interface_remote = new root_interface_class();\n')
        f.write(
            '      root_interface_remote.$.bindNewPipeAndPassReceiver().bindInBrowser(scope);\n'
        )
        f.write('      return root_interface_remote;\n   };\n')
        f.write('  </script>\n')
        f.write(" </body>\n")
        f.write("</html>\n")

    with action_helpers.atomic_output(args.outfile, mode="w") as f:
        f.write('!lineguard try { <line> } catch(e) {}\n')
        f.write('<root root=true> = <lines>\n')
        for rule in builder.rules:
            line = f'<{rule.lhs.name}> = '
            rhs_line = ''
            for elt in rule.rhs:
                if isinstance(elt, str):
                    rhs_line += elt
                else:
                    rhs_line += f'<{elt.name}>'
            f.write(line + rhs_line + '\n')

        f.write('!begin helperlines\n')
        for rule in builder.helperlines:
            assert rule.lhs.should_record
            line = f'<new {rule.lhs.name}> = '
            rhs_line = ''
            for elt in rule.rhs:
                if isinstance(elt, str):
                    rhs_line += elt
                else:
                    rhs_line += f'<{elt.name}>'
            f.write(line + rhs_line + '\n')
        f.write('!end helperlines\n')

        f.write('!begin lines\n')
        for rule in builder.lines:
            line = ''
            if rule.lhs.should_record:
                line = f'<new {rule.lhs.name}> = '
            rhs_line = ''
            for elt in rule.rhs:
                if isinstance(elt, str):
                    rhs_line += elt
                else:
                    rhs_line += f'<{elt.name}>'
            f.write(line + rhs_line + '\n')
        f.write('!end lines\n')


if __name__ == "__main__":
    main()

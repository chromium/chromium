#!/usr/bin/env python3
#
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script generates a Domato grammar from the WebIDL API database.
To do that, it parses the database and builds the javascript boilerplate so
that Domato can interact with the WebIDLs.

Usage: ./generator.py -p /path/to/web_idl_database.pick -o outfile.txt
"""

from __future__ import annotations

import argparse
import os
import sys
from typing import List, Dict, Union, Sequence
import dataclasses
import itertools


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

sys.path.insert(1, os.path.join(SOURCE_DIR, 'third_party'))
sys.path.append(os.path.join(SOURCE_DIR, 'build'))
sys.path.append(
    os.path.join(SOURCE_DIR, 'third_party/blink/renderer/bindings/scripts/'))

import web_idl
import action_helpers

@dataclasses.dataclass
class DomatoType:
  name: str
  should_record: bool = False
  is_terminal: bool = False


@dataclasses.dataclass
class Rule:
  lhs: DomatoType
  rhs: List[Union[str, DomatoType]]


@dataclasses.dataclass
class DomatoGrammarBuilder:

  def __init__(self):
    self.rules: List[Rule] = []
    self.lines: List[Rule] = []
    self.helperlines : List[Rule] = []

  def add_rule(self, rule: Rule):
    self.rules.append(rule)

  def add_line(self, rule: Rule):
    self.lines.append(rule)

  def add_helper_line(self, rule: Rule):
    self.helperlines.append(rule)


# Everything listed here will not get included in the WebIDL grammar, and
# anything that depends on those won't get included as well to avoid
# unresolvable dependencies.
WEBIDL_GRAMMAR_IGNORE_LIST = {
  'Document': {
    'methods': [
      'open'
    ]
  },
  'Window': {
    'methods': [
      'open'
    ]
  }
}


SIMPLE_TYPE_TO_DOMATOTYPE = {
    'void': DomatoType(name='void', is_terminal=True),
    'object': DomatoType(name='object', is_terminal=True),
    'undefined': DomatoType(name='undefined', is_terminal=True),
    'any': DomatoType(name='any', is_terminal=True),
    'byte': DomatoType(name='byte', is_terminal=True),
    'octet': DomatoType(name='octet', is_terminal=True),
    'short': DomatoType(name='short', is_terminal=True),
    'unsigned short': DomatoType(name='unsigned_short', is_terminal=True),
    'long': DomatoType(name='int32', is_terminal=True),
    'unsigned long': DomatoType(name='uint32', is_terminal=True),
    'long long': DomatoType(name='int64', is_terminal=True),
    'unsigned long long': DomatoType(name='uint64', is_terminal=True),
    'integer': DomatoType(name='int16', is_terminal=True),
    'float': DomatoType(name='float', is_terminal=True),
    'double': DomatoType(name='double', is_terminal=True),
    'unrestricted float': DomatoType(name='float', is_terminal=True),
    'unrestricted double': DomatoType(name='double', is_terminal=True),
    'bigint': DomatoType(name='bigint', is_terminal=True),
    'boolean': DomatoType(name='boolean', is_terminal=True),
    'DOMString': DomatoType(name='DOMString', is_terminal=True),
    'ByteString': DomatoType(name='ByteString', is_terminal=True),
    'USVString': DomatoType(name='USVString', is_terminal=True),
    'ArrayBuffer': DomatoType(name='ArrayBuffer',
                              should_record=True, is_terminal=True),
    'ArrayBufferView': DomatoType(name='ArrayBufferView',
                                  should_record=True, is_terminal=True),
    'SharedArray': DomatoType(name='SharedArray',
                              should_record=True, is_terminal=True),
    'Int8Array': DomatoType(name='Int8Array',
                            should_record=True, is_terminal=True),
    'Int16Array': DomatoType(name='Int16Array',
                             should_record=True, is_terminal=True),
    'Int32Array': DomatoType(name='Int32Array',
                             should_record=True, is_terminal=True),
    'Uint8Array': DomatoType(name='Uint8Array',
                             should_record=True, is_terminal=True),
    'Uint16Array': DomatoType(name='Uint16Array',
                              should_record=True, is_terminal=True),
    'Uint32Array': DomatoType(name='Uint32Array',
                              should_record=True, is_terminal=True),
    'Uint8ClampedArray': DomatoType(name='Uint8ClampedArray',
                                    should_record=True, is_terminal=True),
    'BigInt64Array': DomatoType(name='BigInt64Array',
                                should_record=True, is_terminal=True),
    'BigUint64Array': DomatoType(name='BigUint64Array',
                                 should_record=True, is_terminal=True),
    'Float16Array': DomatoType(name='Float16Array',
                               should_record=True, is_terminal=True),
    'Float32Array': DomatoType(name='Float32Array',
                               should_record=True, is_terminal=True),
    'Float64Array': DomatoType(name='Float64Array',
                               should_record=True, is_terminal=True),
    'DataView': DomatoType(name='DataView',
                           should_record=True, is_terminal=True),
}

DEFINED_TYPES = set()

def get_idl_type(builder: DomatoGrammarBuilder,
                 idl_type: web_idl.idl_type.IdlType) -> DomatoType:
  if isinstance(idl_type, web_idl.idl_type.SimpleType):
    return SIMPLE_TYPE_TO_DOMATOTYPE[idl_type.keyword_typename]
  if isinstance(idl_type, web_idl.idl_type.ReferenceType):
    return DomatoType(name=idl_type.identifier, should_record=True)
  if isinstance(idl_type, web_idl.idl_type.UnionType):
    inner_types = [get_idl_type(builder, t) for t in idl_type.member_types]
    typename = 'Or'.join([t.name for t in inner_types])
    if typename in DEFINED_TYPES:
      return DomatoType(name=typename)
    for domato_type in inner_types:
      builder.add_rule(Rule(DomatoType(name=typename), [domato_type]))
    DEFINED_TYPES.add(typename)
    return DomatoType(name=typename)
  if isinstance(idl_type, web_idl.idl_type.NullableType):
    inner_type = get_idl_type(builder, idl_type.inner_type)
    typename = f'{inner_type.name}OrNull'
    if typename in DEFINED_TYPES:
      return DomatoType(name=typename)
    builder.add_rule(Rule(DomatoType(typename), [DomatoType(name='null')]))
    builder.add_rule(Rule(DomatoType(typename), [inner_type]))
    DEFINED_TYPES.add(typename)
    return DomatoType(typename)
  if isinstance(idl_type, web_idl.idl_type._ArrayLikeType):
    # <ArrayLike{ElementName}> = [ <ArrayLike{ElementName}Elements> ]
    # <ArrayLike{ElementName}Elements> =
    # <ArrayLike{ElementName}Elements> =
    #   <ElementName>, <ArrayLike{ElementName}Elements>
    element_type = get_idl_type(builder, idl_type.element_type)
    record_name = f'ArrayLike{element_type.name}'
    if record_name in DEFINED_TYPES:
      return DomatoType(name=record_name, should_record=True)
    DEFINED_TYPES.add(record_name)
    record_elements_type = DomatoType(f'{record_name}Elements')
    rule = ['[', record_elements_type, ']']
    builder.add_rule(Rule(DomatoType(record_name), rule))
    builder.add_rule(Rule(record_elements_type, rhs=['']))
    recursive_rule = [element_type, ', ', record_elements_type]
    builder.add_rule(Rule(record_elements_type, recursive_rule))
    if element_type.should_record:
      rule = [DomatoType(record_name), '[<int min=0 max=10>];']
      builder.add_helper_line(Rule(element_type, rule))
    return DomatoType(name=record_name, should_record=True)
  if isinstance(idl_type, web_idl.idl_type.PromiseType):
    inner = get_idl_type(builder, idl_type.result_type)
    promise_type = DomatoType(f'Promise{inner.name}', should_record=True)
    rule = ['new Promise(() => { return ', inner, '; });']
    builder.add_helper_line(Rule(promise_type, rule))
    return promise_type
  if isinstance(idl_type, web_idl.idl_type.RecordType):
    # We basically need to create the following rules:
    #     <RecordTypeNameElements> = <KeyTypeName>: <ValueTypeName>,
    #         <RecordTypeNameElements>
    #     <RecordTypeNameElements> =
    #     <new RecordTypeName> = { <RecordTypeNameElements> };
    key_type = get_idl_type(builder, idl_type.key_type)
    value_type = get_idl_type(builder, idl_type.value_type)
    record_name = f'Record{key_type.name}{value_type.name}'
    if record_name in DEFINED_TYPES:
      return DomatoType(name=record_name, should_record=True)
    DEFINED_TYPES.add(record_name)
    elements_name = f'{record_name}Elements'
    rule = ['{ ', DomatoType(elements_name), ' };']
    record_rule = Rule(DomatoType(record_name, should_record=True), rule)
    elements_rules = [
        Rule(DomatoType(elements_name), ['']),
        Rule(DomatoType(elements_name), [key_type, ': ', value_type, ', ',
                                         DomatoType(elements_name)]),
    ]
    builder.add_rule(elements_rules[0])
    builder.add_rule(elements_rules[1])
    builder.add_helper_line(record_rule)
    return DomatoType(name=record_name, should_record=True)
  return DomatoType(name='', should_record=False)


def build_argument_rule(
    builder: DomatoGrammarBuilder,
    argument: web_idl.argument.Argument) -> DomatoType:
  if argument.is_variadic:
    idl_type = get_idl_type(builder, argument.idl_type.element_type)
  else:
    idl_type = get_idl_type(builder, argument.idl_type)
  if argument.default_value is not None:
    n = DomatoType(name=f'Argument{argument.identifier}OrDefaultValue')
    builder.add_rule(Rule(n, [argument.default_value.literal]))
    builder.add_rule(Rule(n, [idl_type]))
    return n
  return idl_type


def get_functionlike_types(
    builder: DomatoGrammarBuilder,
    function: web_idl.function_like.FunctionLike) -> Sequence[DomatoType]:
  if function.return_type.is_promise:
    ret_type = get_idl_type(builder, function.return_type.result_type)
  else:
    ret_type = get_idl_type(builder, function.return_type)
  args = [build_argument_rule(builder, arg) for arg in function.arguments]
  return [ret_type] + args


def build_operation_rules(builder: DomatoGrammarBuilder, operation,
                          interface_identifier: str | DomatoType):
  should_await = operation.return_type.is_promise
  pre_rhs = [interface_identifier, f'.{operation.identifier}']
  if should_await:
    pre_rhs = ['await '] + pre_rhs
  f_types = get_functionlike_types(builder, operation)
  lhs = f_types[0]
  combination = list(filter(lambda x: x is not None, f_types[1:]))
  rhs = [', '] * (len(combination) * 2 - 1)
  rhs[0::2] = combination
  builder.add_line(Rule(lhs, pre_rhs + ['('] + rhs + [');']))


def get_methods_to_ignore(
    interface: web_idl.interface.Interface) -> Sequence[str]:
  if not interface.identifier in WEBIDL_GRAMMAR_IGNORE_LIST:
    return []
  return WEBIDL_GRAMMAR_IGNORE_LIST[interface.identifier]['methods']


def build_interface_rules(builder: DomatoGrammarBuilder, interface):
  to_ignore = get_methods_to_ignore(interface)
  non_static_ops = [
      op for op in interface.operations
      if not op.is_static and not op.is_special_operation
  ]
  iface_type = DomatoType(interface.identifier)
  for op in non_static_ops:
    if op.identifier in to_ignore:
      continue
    build_operation_rules(builder, op, iface_type)
  static_ops = [
      op for op in interface.operations
      if op.is_static and not op.is_special_operation
  ]
  for op in static_ops:
    if op.identifier in to_ignore:
      continue
    build_operation_rules(builder, op, interface.identifier)
  for attr in interface.attributes:
    should_await = attr.idl_type.is_promise
    idl_type = attr.idl_type if not should_await else attr.idl_type.result_type
    type = get_idl_type(builder, idl_type)
    rhs = [interface.identifier if attr.is_static else iface_type]
    rhs += [f'.{attr.identifier};']
    if should_await:
      rhs = ['await '] + rhs
    builder.add_line(Rule(type, rhs))
    if attr.is_readonly:
      continue
    # Handle writable attributes now.
    rhs = [interface.identifier if attr.is_static else iface_type]
    rhs += [f'.{attr.identifier}']
    builder.add_line(Rule(DomatoType(''), rhs + [ ' = ', type, ';']))
  for constructor in interface.constructors:
    build_constructor_rules(builder, interface.identifier, constructor)

  getters = [op for op in interface.operations if op.is_getter]
  for getter in getters:
    should_await = getter.return_type.is_promise
    pre_rhs = [iface_type]
    if should_await:
      pre_rhs = ['await '] + pre_rhs
    f_types = get_functionlike_types(builder, getter)
    lhs = f_types[0]
    combination = list(filter(lambda x: x is not None, f_types[1:]))
    rhs = [', '] * (len(combination) * 2 - 1)
    rhs[0::2] = combination
    builder.add_line(Rule(lhs, pre_rhs + ['['] + rhs + ['];']))


def build_constructor_rules(builder: DomatoGrammarBuilder,
                            interface_identifier: str,
                            constructor: web_idl.constructor.Constructor):
  # <new {interface_identifier}> = new {interface_identifier}(args...)
  lhs = DomatoType(interface_identifier, should_record=True)
  f_types = get_functionlike_types(builder, constructor)
  combination = f_types[1:]
  combination = list(filter(lambda x: x is not None, combination))
  rhs = [', '] * (len(combination) * 2 - 1)
  rhs[0::2] = combination
  rule = [f'new {interface_identifier}('] + rhs + [');']
  builder.add_helper_line(Rule(lhs, rule))


def build_dictionary_member(builder: DomatoGrammarBuilder,
                            member: web_idl.dictionary.DictionaryMember):
  idl_type = get_idl_type(builder, member.idl_type)
  if member.default_value:
    n = DomatoType(f'DictionaryMember{member.identifier}OrDefaultValue')
    builder.add_rule(Rule(n, [member.default_value.literal]))
    builder.add_rule(Rule(n, [idl_type]))
    return n
  return idl_type


def build_dictionary_rules(builder: DomatoGrammarBuilder,
                           dictionary: web_idl.dictionary.Dictionary):
  # Dictionaries are declared like this:
  #     <new DictionaryName> = { "member1.identifier": <Member1TypeName>, ... }
  members = dictionary.members
  combination = [build_dictionary_member(builder, member) for member in members]
  identifiers = [m.identifier for m in dictionary.members]
  lhs = DomatoType(dictionary.identifier, should_record=True)
  rhs = itertools.chain.from_iterable(
    [(f'"{identifiers[id]}": ', c, ', ') for id, c in enumerate(combination)]
  )
  rhs = list(rhs)
  if rhs:
    rhs.pop()
  rhs = ['{'] + rhs + ['};']
  builder.add_helper_line(Rule(lhs, rhs))
  for member in dictionary.members:
    if not member.is_required:
      continue
    type = get_idl_type(builder, member.idl_type)
    if type.should_record:
      rule = [DomatoType(dictionary.identifier), f'.{member.identifier};']
      builder.add_helper_line(Rule(type, rule))


def build_enumeration_rules(builder: DomatoGrammarBuilder,
                            enumeration: web_idl.enumeration.Enumeration):
  lhs = DomatoType(enumeration.identifier)
  for value in enumeration.values:
    builder.add_rule(Rule(lhs, rhs=[f'"{value}"']))
  builder.add_rule(Rule(lhs, ["\"<string>\""]))


def build_typedef_rules(builder: DomatoGrammarBuilder,
                        typedef: web_idl.typedef.Typedef):
  type = get_idl_type(builder, typedef.idl_type)
  builder.add_rule(Rule(lhs=DomatoType(typedef.identifier), rhs=[type]))
  if type.should_record:
    builder.add_rule(Rule(lhs=type, rhs=[DomatoType(typedef.identifier)]))


def construct_functionlike(
    builder: DomatoGrammarBuilder,
    function: web_idl.function_like.FunctionLike) -> Sequence[str]:
  res = []
  f_types = get_functionlike_types(builder, function)
  combination = list(filter(lambda x: x is not None, f_types[1:]))
  arg_s = ", ".join(f'/*{c.name}*/ arg{id}'
                    for id, c in enumerate(combination))
  res.append(f'({arg_s}) => {{}}')
  return res


def build_callback_function_rules(
    builder: DomatoGrammarBuilder,
    cb: web_idl.callback_function.CallbackFunction):
  for construct in construct_functionlike(builder, cb):
    builder.add_rule(Rule(DomatoType(cb.identifier), [construct]))


def build_callback_interface_rules(
    builder: DomatoGrammarBuilder,
    interface: web_idl.callback_interface.CallbackInterface):
  # <new {iface.identifier}> = {'memberfunc1': () => {}, 'member2': <Member2>}
  builder.add_rule(Rule(DomatoType(interface.identifier), ['{}']))


def remove_cyclic_dependencies(builder: DomatoGrammarBuilder):
  graph = {}
  backrefs = {}
  for rule in builder.rules + builder.lines + builder.helperlines:
    assert isinstance(graph, dict)
    if not rule.lhs.name in graph:
      graph[rule.lhs.name] = []
    deps = []
    for type in rule.rhs:
      if not isinstance(type, DomatoType) or type.is_terminal:
        continue
      deps.append(type.name)
      if not type.name in backrefs:
        backrefs[type.name] = set()
      backrefs[type.name].add(rule.lhs.name)
    graph[rule.lhs.name].append(deps)

  handled = dict()

  # Some of the types handled by default.
  for key in SIMPLE_TYPE_TO_DOMATOTYPE.keys():
    handled[key] = True
  handled['null'] = True

  def _check(name) -> bool:
    if name in handled:
      return handled[name]
    handled[name] = False
    if not name in graph:
      handled[name] = False
      return False

    # we need to have at least one path to be true.
    res = False
    for elts in graph[name]:
      res |= all(_check(child) for child in elts)
    handled[name] = res
    return res
  for name in graph.keys():
    _check(name)
    for deps in graph[name]:
      for dep in deps:
        _check(dep)

  while True:
    cpy = handled.copy()
    for name, val in cpy.items():
      if not val:
        handled.pop(name)
        _check(name)
    if cpy == handled:
      break

  # Those predefined types are refering to the `Internal` WebIDL that's only
  # exposed in tests. It pollutes our grammar, and we don't want to fuzz that.
  to_remove = {
    'InternalDictionary',
    'InternalDictionaryDerived',
    'InternalDictionaryDerivedDerived'
  }
  for name, val in handled.items():
    if not val:
      to_remove.add(name)

  def filter_rule(rule: Rule):
    if rule.lhs.name in to_remove:
      return False
    for elt in rule.rhs:
      if not isinstance(elt, DomatoType):
        continue
      if elt.name in to_remove:
        return False
    return True

  builder.rules = list(filter(filter_rule, builder.rules))
  builder.lines = list(filter(filter_rule, builder.lines))
  builder.helperlines = list(filter(filter_rule, builder.helperlines))


def main():
  parser = argparse.ArgumentParser(
      description=
      'Generates a grammar for Domato that describes WebIDLs.')
  parser.add_argument('-p',
                      '--path',
                      required=True,
                      help="Path to the web_idl_database.")
  parser.add_argument('-i',
                      '--include_path',
                      required=False,
                      help="Path to the grammar helper file.")
  parser.add_argument('-o',
                      '--outfile',
                      required=True,
                      help="Path to the output profile.")

  args = parser.parse_args()
  database = web_idl.Database.read_from_file(args.path)

  builder = DomatoGrammarBuilder()
  rules: Dict[str, List[str]] = {}
  rules['lines'] = list()
  for interface in database.interfaces:
    build_interface_rules(builder, interface)
  for dictionary in database.dictionaries:
    build_dictionary_rules(builder, dictionary)
  for enumeration in database.enumerations:
    build_enumeration_rules(builder, enumeration)
  for typedef in database.typedefs:
    build_typedef_rules(builder, typedef)
  for cb in database.callback_functions:
    build_callback_function_rules(builder, cb)
  for interface in database.callback_interfaces:
    build_callback_interface_rules(builder, interface)

  for interface in database.interfaces:
    ifc_type = DomatoType(interface.identifier)
    for derived in interface.subclasses:
      drv_type = DomatoType(derived.identifier)
      builder.add_rule(Rule(ifc_type, [drv_type]))
      builder.add_rule(Rule(drv_type, [ifc_type]))

  remove_cyclic_dependencies(builder)
  with action_helpers.atomic_output(args.outfile, mode="w") as f:
    if args.include_path:
      f.write(f'!include {args.include_path}\n')
    f.write('!lineguard try { <line> } catch(e) { }\n')
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

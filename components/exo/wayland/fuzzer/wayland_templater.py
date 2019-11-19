# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Templatize a file based on wayland specifications.

The templating engine takes an input template and one or more wayland
specifications (see third_party/wayland/src/protocol/wayland.dtd), and
instantiates the template based on the wayland content.
"""

from __future__ import absolute_import
from __future__ import print_function

import os
import sys

import jinja2
import wayland_utils as wlu

proto_type_conversions = {
    'array': 'bytes',
    'fixed': 'double',
    'fd': 'small_value',
    'int': 'int32',
    'new_id': None,
    'object': 'small_value',
    'string': 'string',
    'uint': 'uint32',
}

cpp_type_conversions = {
    'array': 'struct wl_array*',
    'fd': 'int',
    'fixed': 'wl_fixed_t',
    'int': 'int32_t',
    'string': 'const char*',
    'uint': 'uint32_t',
}


def GetCppPtrType(interface_name):
  """Returns the c++ type associated with interfaces of the given name.

  Args:
    interface_name: the name of the interface you want the type for, or None.

  Returns:
    the c++ type which wayland will generate for this interface, or void* if
    the interface_name is none. We use "struct foo*" due to a collision between
    typenames and function names (specifically, wp_presentation has a feedback()
    method and there is also a wp_presentation_feedback interface).
  """
  if not interface_name:
    return 'void*'
  return 'struct ' + interface_name + '*'


def GetCppType(arg):
  ty = arg.attrib['type']
  if ty in ['object', 'new_id']:
    return GetCppPtrType(arg.get('interface'))
  return cpp_type_conversions[ty]


class TemplaterContext(object):
  """The context object used for recording stateful/expensive things.

  An instance of this class is used when generating the template data, we use
  it to cache pre-computed information, as well as to side-effect stateful
  queries (such as counters) while generating the template data.
  """

  def __init__(self, protocols):
    self.non_global_names = {
        wlu.GetConstructedInterface(m) for _, _, m in wlu.AllMessages(protocols)
    } - {None}
    self.interfaces_with_listeners = {
        i.attrib['name']
        for p, i in wlu.AllInterfaces(protocols)
        if wlu.NeedsListener(i)
    }
    self.counts = {}

  def GetAndIncrementCount(self, counter):
    """Return the number of times the given key has been counted.

    Args:
      counter: the key used to identify this count value.

    Returns:
      An int which is the number of times this method has been called before
      with this counter's key.
    """
    self.counts[counter] = self.counts.get(counter, 0) + 1
    return self.counts[counter] - 1


def GetArg(arg):
  ty = arg.attrib['type']
  return {
      'name': arg.attrib['name'],
      'type': ty,
      'nullable': arg.get('allow-null', 'false') == 'true',
      'proto_type': proto_type_conversions[ty],
      'cpp_type': GetCppType(arg),
      'interface': arg.get('interface'),
      'doc': wlu.GetDocumentation(arg),
  }


def GetMessage(message, context):
  name = message.attrib['name']
  constructed = wlu.GetConstructedInterface(message)
  return {
      'name':
          name,
      'tag':
          message.tag,
      'idx':
          context.GetAndIncrementCount('message_index'),
      'args': [GetArg(a) for a in message.findall('arg')],
      'is_constructor':
          wlu.IsConstructor(message),
      'is_destructor':
          wlu.IsDestructor(message),
      'constructed':
          constructed,
      'constructed_has_listener':
          constructed in context.interfaces_with_listeners,
      'doc':
          wlu.GetDocumentation(message),
  }


def GetInterface(interface, context):
  name = interface.attrib['name']
  return {
      'name':
          name,
      'idx':
          context.GetAndIncrementCount('interface_index'),
      'cpp_type':
          GetCppPtrType(name),
      'is_global':
          name not in context.non_global_names,
      'events': [GetMessage(m, context) for m in interface.findall('event')],
      'requests': [
          GetMessage(m, context) for m in interface.findall('request')
      ],
      'has_listener':
          wlu.NeedsListener(interface),
      'doc':
          wlu.GetDocumentation(interface),
  }


def GetTemplateData(protocol_paths):
  protocols = wlu.ReadProtocols(protocol_paths)
  context = TemplaterContext(protocols)
  interfaces = []
  for p in protocols:
    for i in p.findall('interface'):
      interfaces.append(GetInterface(i, context))
  assert all(p.endswith('.xml') for p in protocol_paths)
  return {
      'protocol_names': [str(os.path.basename(p))[:-4] for p in protocol_paths],
      'interfaces':
          interfaces,
  }


def InstantiateTemplate(in_tmpl, in_ctx, output, in_directory):
  env = jinja2.Environment(
      loader=jinja2.FileSystemLoader(in_directory),
      keep_trailing_newline=True,  # newline-terminate generated files
      lstrip_blocks=True,
      trim_blocks=True)  # so don't need {%- -%} everywhere
  template = env.get_template(in_tmpl)
  with open(output, 'w') as out_fi:
    out_fi.write(template.render(in_ctx))


def main(argv):
  """Execute the templater, based on the user provided args.

  Args:
    argv: the command line arguments (including the script name)
  """
  parsed_args = wlu.ParseOpts(argv)
  InstantiateTemplate(parsed_args.input, GetTemplateData(parsed_args.spec),
                      parsed_args.output, parsed_args.directory)


if __name__ == '__main__':
  main(sys.argv)

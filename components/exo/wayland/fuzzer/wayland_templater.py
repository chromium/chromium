# Copyright 2019 The Chromium Authors
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
import platform as platform_module
import subprocess
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


def GetClangFormatPath():
    """Returns the path to clang-format, for formatting the output."""
    new_path_platform_suffix = ''
    if sys.platform.startswith('linux'):
      platform, exe_suffix = 'linux64', ''
      exe_suffix = ""
    elif sys.platform == 'darwin':
      platform, exe_suffix = 'mac', ''
      host_arch = platform_module.machine().lower()
      if host_arch == 'arm64' or host_arch.startswith('aarch64'):
        new_path_platform_suffix = '_arm64'
    elif sys.platform == 'win32':
      platform, exe_suffix = 'win', '.exe'
    else:
      assert False, 'Unknown platform: ' + sys.platform

    this_dir = os.path.abspath(os.path.dirname(__file__))
    root_src_dir = os.path.abspath(
        os.path.join(this_dir, '..', '..', '..', '..'))
    buildtools_platform_dir = os.path.join(root_src_dir, 'buildtools', platform)
    new_buildtools_platform_dir = os.path.join(
        root_src_dir, 'buildtools', platform + new_path_platform_suffix)
    # TODO(b/328065301): Remove old paths once clang hooks are migrated
    possible_paths = [
     os.path.join(
        buildtools_platform_dir, 'clang-format' + exe_suffix),
     os.path.join(
        new_buildtools_platform_dir, 'format', 'clang-format' + exe_suffix),
     os.path.join(
        f'{new_buildtools_platform_dir}-format', 'clang-format' + exe_suffix),
    ]
    for path in possible_paths:
      if os.path.isfile(path):
        return path


def ClangFormat(source, filename):
  """Runs clang-format on source and returns the result."""
  # clang-format the output, for better readability and for
  # -Wmisleading-indentation.
  clang_format_cmd = [GetClangFormatPath(), '--assume-filename=' + filename]
  proc = subprocess.Popen(
      clang_format_cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
  stdout_output, stderr_output = proc.communicate(input=source.encode('utf8'))
  retcode = proc.wait()
  if retcode != 0:
      raise CalledProcessError(retcode, 'clang-format error: ' + stderr_output)
  return stdout_output.decode()


def WriteIfChanged(contents, filename):
  """Writes contents to filename.

  If filename already has the right contents, nothing is written so that
  the mtime on filename doesn't change.
  """
  if os.path.exists(filename):
    with open(filename, 'r') as in_fi:
      if in_fi.read() == contents:
        return
  with open(filename, 'w') as out_fi:
    out_fi.write(contents)


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
  raw_output = template.render(in_ctx)

  # For readability, and for -Wmisleading-indentation.
  if output.endswith(('.h', '.cc', '.proto')):
    formatted_output = ClangFormat(raw_output, filename=output)
  else:
    formatted_output = raw_output

  WriteIfChanged(formatted_output, filename=output)


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

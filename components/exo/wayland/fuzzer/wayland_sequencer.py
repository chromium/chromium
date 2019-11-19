# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generate sequences of valid wayland instructions.

Determines sequences of calls necessary to instantiate and invoke a given
message of one of the wayland interfaces. The procedure is as follows:
  - Get a dependency graph between messages, and the interfaces you need in
    order to invoke them.
  - The list of messages you need to invoke is the sum of transitive
    dependencies for the target message.
"""

from __future__ import absolute_import
from __future__ import print_function

import os
import sys

import wayland_templater
import wayland_utils as wlu

# To prevent the fuzzer from exploring too-much space, we limit it to binding at
# most a small number of each interface. So the index has to be in this map.
small_value = {0: 'ZERO', 1: 'ONE', 2: 'TWO', 3: 'THREE'}


def ToNode(interface, message):
  return (interface.attrib['name'], message.attrib['name'])


def GetDependencyGraph(protocols):
  """Determine the dependencies between interfaces and their messages.

  Args:
    protocols: the list of wayland xml protocols you want the dependencies of.

  Returns:
    A bipartite graph where messages (i, m) depend on interfaces (i) and
    vice-versa. An edge from (i,m) to (i') indicates an i' instance is needed to
    invoke m, whereas (i) to (i',m') indicates m' is a constructor for i
    instances.
  """
  dep_graph = {}
  constructed_interfaces = set()
  for _, i, m in wlu.AllMessages(protocols):
    dep_graph[ToNode(i, m)] = [('receiver', i.attrib['name'])] + [
        (a.attrib['name'], a.get('interface', '?'))
        for a in m.findall('arg')
        if a.attrib['type'] == 'object'
    ]
    constructed = wlu.GetConstructedInterface(m)
    if constructed:
      constructed_interfaces.add(constructed)
      dep_graph[constructed] = ToNode(i, m)
  for _, i in wlu.AllInterfaces(protocols):
    if i.attrib['name'] not in constructed_interfaces:
      dep_graph[i.attrib['name']] = ('wl_registry', 'bind')
  return dep_graph


class SequenceBuilder(object):
  """Store for data used when building one sequence.

  This class is used to build the sequences.  You should make a new
  SequenceBuilder for every sequence you want to generate.
  """

  def __init__(self, name, dep_graph):
    self.name = name
    self.dep_graph = dep_graph
    self.counts = {}
    self.prevented = set()
    self.sequence = []
    # To simulate what the harness itself does, we make a singleton wl_display.
    self.RecordInterfaceCreated('wl_display')
    self.Prevent('wl_display')

  def IsPrevented(self, interface):
    return interface in self.prevented

  def Prevent(self, interface):
    self.prevented.add(interface)

  def RecordInterfaceCreated(self, interface):
    self.counts[interface] = self.counts.get(interface, -1) + 1

  def GetLastInterfaceCreated(self, interface):
    """Return the small_value index for the currently available interface.

    Args:
      interface: the name of the interface you want an index of.

    Returns:
      A small_value index for the topmost-version of the interface.
    """
    return small_value[self.counts[interface]]

  def AppendCall(self, interface, message, args):
    self.sequence.append((interface, message, args))

  def AppendRoundTrip(self):
    self.AppendCall('', '', [])

  def BuildInterface(self, interface):
    if not self.IsPrevented(interface):
      if interface == 'wl_registry' or self.dep_graph[interface] == (
          'wl_registry', 'bind'):
        self.Prevent(interface)
      (cons_i, cons_m) = self.dep_graph[interface]
      self.BuildMessage(cons_i, cons_m, interface)
      self.RecordInterfaceCreated(interface)
    return self.GetLastInterfaceCreated(interface)

  def BuildMessage(self, interface, message, target_i):
    """Build the message sequence up to and including the supplied message.

    Args:
      interface: the name of the interface that defines the message you want to
        send.
      message: the name of the message you want to send.
      target_i: the interface you expect to be created by this message (if there
        is one, otherwise use '').
    """
    args = [(arg_name, self.BuildInterface(arg_type))
            for arg_name, arg_type in self.dep_graph[(interface, message)]]
    if interface == 'wl_registry' and message == 'bind' and target_i:
      args.append(('global', target_i))
    self.AppendCall(interface, message, args)
    # We need to do a round-trip after binding the registry so that we have the
    # globals available for binding.
    if interface == 'wl_display' and message == 'get_registry' and target_i:
      self.AppendRoundTrip()

  def TemplateData(self):
    return {'sequence': self.sequence}


def GetManualSequences(dep_graph):
  """Get the sequences which can't be automated.

  Some sequences of messages are interesting, and involve more than what can be
  automatically generated by looking at the dependency graph. In such cases we
  define the sequence here so that it will still be automatically generated as
  part of the corpus.

  Args:
    dep_graph: the dependency graph between messages and interfaces.

  Returns:
    A list of SequenceBuilder objects, each one containing a manually-created
    sequence.
  """
  c = SequenceBuilder('copy_paste', dep_graph)
  c_device = c.BuildInterface('wl_data_device')
  c_source = c.BuildInterface('wl_data_source')
  # TODO(crbug/979456): make a fuzz-dictionary with all the mime-types.
  c.AppendCall('wl_data_source', 'offer', [('receiver', c_source),
                                           ('mime_type', '"text/plain"')])
  c.AppendCall('wl_data_device', 'set_selection', [('receiver', c_device),
                                                   ('source', c_source)])
  c.AppendRoundTrip()
  c_surface = c.BuildInterface('wl_shell_surface')
  c.AppendCall('wl_shell_surface', 'set_toplevel', [('receiver', c_surface)])
  c_shm = c.BuildInterface('wl_shm')
  c.AppendRoundTrip()  # Round trip so server can send us formats.
  c.AppendCall('wl_shm', 'create_pool', [('receiver', c_shm), ('size', '64')])
  c.RecordInterfaceCreated('wl_shm_pool')
  c_shm_pool = c.GetLastInterfaceCreated('wl_shm_pool')
  c.AppendCall('wl_shm_pool', 'create_buffer', [('receiver', c_shm_pool),
                                                ('width', 1), ('stride', 4),
                                                ('height', 1)])
  c.RecordInterfaceCreated('wl_buffer')
  c_buffer = c.GetLastInterfaceCreated('wl_buffer')
  c.AppendCall('wl_surface', 'attach', [('receiver', c_surface),
                                        ('buffer', c_buffer)])
  c.AppendCall('wl_surface', 'damage', [('receiver', c_surface), ('width', 1),
                                        ('height', 1)])
  c.AppendCall('wl_surface', 'commit', [('receiver', c_surface)])
  c.AppendRoundTrip()  # Round trip so server can make the data_offer.
  c.RecordInterfaceCreated('wl_data_offer')

  e = SequenceBuilder('empty', dep_graph)

  p = SequenceBuilder('popup_configuration', dep_graph)
  p_positioner = p.BuildInterface('zxdg_positioner_v6')
  p_parent = p.BuildInterface('zxdg_toplevel_v6')
  p.AppendCall('zxdg_surface_v6', 'set_window_geometry',
               [('receiver', p_parent), ('x', 0), ('y', 0), ('width', 10),
                ('height', 10)])
  p.AppendRoundTrip()
  p.AppendCall('wl_surface', 'commit', [('receiver', p_parent)])
  p.AppendRoundTrip()
  p.AppendCall('zxdg_surface_v6', 'ack_configure', [('receiver', p_parent),
                                                    ('serial', 1)])
  p_child = p.BuildInterface('zxdg_surface_v6')
  p.AppendCall('zxdg_surface_v6', 'get_popup', [('receiver', p_child),
                                                ('parent', p_parent),
                                                ('positioner', p_positioner)])
  p.AppendRoundTrip()

  return [c, e, p]


def SequenceToTemplate(parsed_arguments, builder):
  out_dir = parsed_arguments.output
  if not os.path.exists(out_dir):
    os.mkdir(out_dir)
  out_path = os.path.join(out_dir, builder.name + '.asciipb')
  wayland_templater.InstantiateTemplate(parsed_arguments.input,
                                        builder.TemplateData(), out_path,
                                        parsed_arguments.directory)


def Main(argv):
  """Instantiate the group of message-sequences used to seed the fuzzer.

  Args:
    argv: command-line arguments used to run the sequencer.
  """
  parsed = wlu.ParseOpts(argv)

  protocols = wlu.ReadProtocols(parsed.spec)
  dep_graph = GetDependencyGraph(protocols)
  for _, interface in wlu.AllInterfaces(protocols):
    for req in interface.findall('request'):
      interface_name = interface.attrib['name']
      message_name = req.attrib['name']
      builder = SequenceBuilder(interface_name + '_' + message_name, dep_graph)
      builder.BuildMessage(interface_name, message_name, '')

      # Add a round-trip to the sequence in case the server wants to do
      # something funky.
      builder.AppendRoundTrip()

      SequenceToTemplate(parsed, builder)

  # For sequences which are more complicated than a dependency search, we have
  # a manual backup.
  for sequence_builder in GetManualSequences(dep_graph):
    SequenceToTemplate(parsed, sequence_builder)


if __name__ == '__main__':
  Main(sys.argv)

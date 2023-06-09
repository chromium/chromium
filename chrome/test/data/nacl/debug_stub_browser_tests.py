# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import sys
import xml.etree.ElementTree

import gdb_rsp


def AssertRaises(exc_class, func):
  try:
    func()
  except exc_class:
    pass
  else:
    raise AssertionError('Function did not raise %r' % exc_class)


def GetTargetArch(connection):
  """Get the CPU architecture of the NaCl application."""
  reply = connection.RspRequest('qXfer:features:read:target.xml:0,fff')
  assert reply[0] == 'l', reply
  tree = xml.etree.ElementTree.fromstring(reply[1:])
  arch_tag = tree.find('architecture')
  assert arch_tag is not None, reply
  return arch_tag.text.strip()


def ReverseBytes(byte_string):
  """Reverse bytes in the hex string: '09ab' -> 'ab09'. This converts
  little-endian number in the hex string to its normal string representation.
  """
  assert len(byte_string) % 2 == 0, byte_string
  return ''.join([byte_string[i - 2 : i]
                  for i in range(len(byte_string), 0, -2)])


def GetProgCtrString(connection, arch):
  """Get current execution point."""
  registers = connection.RspRequest('g')
  # PC register indices can be found in
  # native_client/src/trusted/debug_stub/abi.cc in AbiInit function.
  if arch == 'i386':
    # eip index is 8
    return ReverseBytes(registers[8 * 8 : 8 * 8 + 8])
  if arch == 'i386:x86-64':
    # rip index is 16
    return ReverseBytes(registers[16 * 16 : 16 * 16 + 8])
  if arch == 'iwmmxt':
    # pc index is 15
    return ReverseBytes(registers[15 * 8 : 15 * 8 + 8])
  raise AssertionError('Unknown architecture: %s' % arch)


def TestContinue(connection):
  # Once the NaCl test module reports that the test passed, the NaCl <embed>
  # element is removed from the page.  The NaCl module will be killed by the
  # browser which will appear as EOF (end-of-file) on the debug stub socket.
  AssertRaises(gdb_rsp.EofOnReplyException,
               lambda: connection.RspRequest('vCont;c'))


def TestBreakpoint(connection):
  # Breakpoints and single-stepping might interfere with Chrome sandbox. So we
  # check that they work properly in this test.
  arch = GetTargetArch(connection)
  registers = connection.RspRequest('g')
  pc = GetProgCtrString(connection, arch)
  # Set breakpoint
  result = connection.RspRequest('Z0,%s,1' % pc)
  assert result == 'OK', result
  # Check that we stopped at breakpoint
  result = connection.RspRequest('vCont;c')
  stop_reply = re.compile(r'T05thread:(\d+);')
  assert stop_reply.match(result), result
  thread = stop_reply.match(result).group(1)
  # Check that registers haven't changed
  result = connection.RspRequest('g')
  assert result == registers, (result, registers)
  # Remove breakpoint
  result = connection.RspRequest('z0,%s,1' % pc)
  assert result == 'OK', result
  # Check single stepping
  result = connection.RspRequest('vCont;s:%s' % thread)
  assert result == 'T05thread:%s;' % thread, result
  assert pc != GetProgCtrString(connection, arch)
  # Check that we terminate normally
  AssertRaises(gdb_rsp.EofOnReplyException,
               lambda: connection.RspRequest('vCont;c'))


def Main(args):
  port = int(args[0])
  name = args[1]
  connection = gdb_rsp.GdbRspConnection(('localhost', port))
  if name == 'continue':
    TestContinue(connection)
  elif name == 'breakpoint':
    TestBreakpoint(connection)
  else:
    raise AssertionError('Unknown test name: %r' % name)


if __name__ == '__main__':
  Main(sys.argv[1:])

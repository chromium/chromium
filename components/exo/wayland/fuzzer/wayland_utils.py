# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for working with wayland protocols.

A collection of all the generically useful wayland utilities.
"""

from __future__ import absolute_import
from __future__ import print_function

import argparse
from xml.etree import ElementTree


def ReadProtocols(protocol_paths):
  return [ElementTree.parse(path).getroot() for path in protocol_paths]


def AllInterfaces(protocols):
  """Get the interfaces in these protocols.

  Args:
    protocols: the list of protocols you want the interfaces of.

  Yields:
    Tuples (p, i) of (p)rotocol (i)nterface.
  """
  for p in protocols:
    for i in p.findall('interface'):
      yield (p, i)


def AllMessages(protocols):
  """Get the messages in these protocols.

  Args:
    protocols: the list of protocols you want the messages of.

  Yields:
    Tuples (p, i, m) of (p)rotocol, (i)nterface, and (m)essage.
  """
  for (p, i) in AllInterfaces(protocols):
    for r in i.findall('request'):
      yield (p, i, r)
    for e in i.findall('event'):
      yield (p, i, e)


def GetConstructorArg(message):
  """Get the argument that this message constructs, or None.

  Args:
    message: the message which you want to find the constructor arg of.

  Returns:
    The argument (as an ElementTree node) that constructs a new interface, or
    None.
  """
  return message.find('arg[@type="new_id"]')


def IsConstructor(message):
  """Check if a message is a constructor.

  Args:
    message: the message which you want to check.

  Returns:
    True if the message constructs an object (via new_id), False otherwise.
  """
  return GetConstructorArg(message) is not None


def IsDestructor(message):
  """Check if a message is a destructor.

  Args:
    message: the message which you want to check.

  Returns:
    True if the message has the type='destructor' attribute, false otherwise.
  """
  return message.get('type') == 'destructor'


def GetConstructedInterface(message):
  """Gets the interface constructed by a message.

  Note that even if IsConstructor(message) returns true, get_constructed can
  still return None when the message constructs an unknown interface (e.g.
  wl_registry.bind()).

  Args:
    message: the event/request which may be a constructor.

  Returns:
    The name of the constructed interface (if there is one), or None.
  """
  cons_arg = GetConstructorArg(message)
  if cons_arg is None:
    return None
  return cons_arg.get('interface')


def NeedsListener(interface):
  return interface.find('event') is not None


def GetDocumentation(element):
  """Return this element's documentation as a list of strings.

  Args:
    element: the xml.etree.ElementTree node you want the documentation for.

  Returns:
    A list of strings, containing the data from this node's "summary" attribute,
    or its "description" subelement.
  """
  doc = element.find('description')
  if doc is None:
    summary = element.get('summary')
    return [summary] if summary is not None else []
  ret = [doc.get('summary')]
  if doc.text:
    ret += [l.strip() for l in doc.text.split('\n')]
    # Remove blank lines from the tail only.
    while not ret[-1]:
      ret.pop()
  return ret


def ParseOpts(argv):
  """Parses the given command line arguments for the templater.

  Args:
    argv: the arguments to be parsed.

  Returns:
    An argparse.ArgumentParser which provides the user's chosen configuration
    for this templater run.
  """
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-d',
      '--directory',
      help='treat input paths as relative to this directory',
      default='.')
  parser.add_argument(
      '-i',
      '--input',
      help='path to the input template file (relative to -d)',
      required=True)
  parser.add_argument(
      '-o',
      '--output',
      help='path to write the generated file to',
      required=True)
  parser.add_argument(
      '-s',
      '--spec',
      help='path(s) to the wayland specification(s)',
      nargs='+',
      required=True)
  return parser.parse_args(argv[1:])

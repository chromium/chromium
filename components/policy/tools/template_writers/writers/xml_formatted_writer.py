#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from writers import template_writer


class XMLFormattedWriter(template_writer.TemplateWriter):
  '''Helper class for generating XML-based templates.
  '''

  def AddElement(self, parent, name, attrs=None, text=None):
    '''
    Adds a new XML Element as a child to an existing element or the Document.

    Args:
      parent: An XML element or the document, where the new element will be
        added.
      name: The name of the new element.
      attrs: A dictionary of the attributes' names and values for the new
        element.
      text: Text content for the new element.

    Returns:
      The created new element.
    '''
    if attrs == None:
      attrs = {}

    doc = parent.ownerDocument
    element = doc.createElement(name)
    for key, value in sorted(attrs.items()):
      element.setAttribute(key, value)
    if text:
      element.appendChild(doc.createTextNode(text))
    parent.appendChild(element)
    return element

  def AddText(self, parent, text):
    '''Adds text to a parent node.
    '''
    doc = parent.ownerDocument
    parent.appendChild(doc.createTextNode(text))

  def AddAttribute(self, parent, name, value):
    '''Adds a new attribute to the parent Element. If an attribute with the
    given name already exists then it will be replaced.
    '''
    doc = parent.ownerDocument
    attribute = doc.createAttribute(name)
    attribute.value = value
    parent.setAttributeNode(attribute)

  def AddComment(self, parent, comment):
    '''Adds a comment node.'''
    parent.appendChild(parent.ownerDocument.createComment(comment))

  def ToPrettyXml(self, doc, **kwargs):
    # return doc.toprettyxml(indent='  ')
    # The above pretty-printer does not print the doctype and adds spaces
    # around texts, e.g.:
    #  <string>
    #    value of the string
    #  </string>
    # This is problematic both for the OSX Workgroup Manager (plist files) and
    # the Windows Group Policy Editor (admx files). What they need instead:
    #  <string>value of string</string>
    # So we use a hacky pretty printer here. It assumes that there are no
    # mixed-content nodes.
    # Get all the XML content in a one-line string.
    xml = doc.toxml(**kwargs)
    # Determine where the line breaks will be. (They will only be between tags.)
    lines = xml[1:len(xml) - 1].split('><')
    indent = ''
    res = ''
    # Determine indent for each line.
    for i, line in enumerate(lines):
      if line[0] == '/':
        # If the current line starts with a closing tag, decrease indent before
        # printing.
        indent = indent[2:]
      lines[i] = indent + '<' + line + '>'
      if (line[0] not in ['/', '?', '!'] and '</' not in line and
          line[len(line) - 1] != '/'):
        # If the current line starts with an opening tag and does not conatin a
        # closing tag, increase indent after the line is printed.
        indent += '  '
    # Reconstruct XML text from the lines.
    return '\n'.join(lines)

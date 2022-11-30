#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from writers import template_writer


def GetWriter(config):
  '''Factory method for instanciating the GoogleADMXWriter. Every Writer needs a
  GetWriter method because the TemplateFormatter uses this method to
  instantiate a Writer.
  '''
  return GoogleADMXWriter(None, config)  # platforms unused


class GoogleADMXWriter(template_writer.TemplateWriter):
  '''Simple writer that writes fixed google.admx files.
  '''

  def WriteTemplate(self, template):
    '''Returns the contents of the google.admx file. It's independent of
      policy_templates.json.
    '''

    return '''<?xml version="1.0" ?>
<policyDefinitions revision="1.0" schemaVersion="1.0">
  <policyNamespaces>
    <target namespace="Google.Policies" prefix="Google"/>
  </policyNamespaces>
  <resources minRequiredRevision="1.0" />
  <categories>
    <category displayName="$(string.google)" name="Cat_Google"/>
  </categories>
</policyDefinitions>
'''

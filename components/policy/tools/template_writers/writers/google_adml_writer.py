#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from writers import template_writer


def GetWriter(config):
  '''Factory method for instanciating the GoogleADMLWriter. Every Writer needs a
  GetWriter method because the TemplateFormatter uses this method to
  instantiate a Writer.
  '''
  return GoogleADMLWriter(None, config)  # platforms unused


class GoogleADMLWriter(template_writer.TemplateWriter):
  '''Simple writer that writes fixed google.adml files.
  '''

  def WriteTemplate(self, template):
    '''Returns the contents of the google.adml file. It's independent of
      policy_templates.json.
    '''

    return '''<?xml version="1.0" ?>
<policyDefinitionResources revision="1.0" schemaVersion="1.0">
  <displayName/>
  <description/>
  <resources>
    <stringTable>
      <string id="google">Google</string>
    </stringTable>
  </resources>
</policyDefinitionResources>
'''

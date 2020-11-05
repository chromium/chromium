# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from copy import copy
import logging
import posixpath

from api_models import GetNodeCategories
from api_schema_graph import APINodeCursor
from docs_server_utils import MarkFirstAndLast
from extensions_paths import JSON_TEMPLATES, PRIVATE_TEMPLATES
from operator import itemgetter
from platform_util import PlatformToExtensionType
import third_party.json_schema_compiler.model as model


def CreateSamplesView(samples_list, request):
  def get_sample_id(sample_name):
    return sample_name.lower().replace(' ', '-')

  def get_accepted_languages(request):
    if request is None:
      return []
    accept_language = request.headers.get('Accept-Language', None)
    if accept_language is None:
      return []
    return [lang_with_q.split(';')[0].strip()
            for lang_with_q in accept_language.split(',')]

  return_list = []
  for dict_ in samples_list:
    name = dict_['name']
    description = dict_['description']
    if description is None:
      description = ''
    if name.startswith('__MSG_') or description.startswith('__MSG_'):
      try:
        # Copy the sample dict so we don't change the dict in the cache.
        sample_data = dict_.copy()
        name_key = name[len('__MSG_'):-len('__')]
        description_key = description[len('__MSG_'):-len('__')]
        locale = sample_data['default_locale']
        for lang in get_accepted_languages(request):
          if lang in sample_data['locales']:
            locale = lang
            break
        locale_data = sample_data['locales'][locale]
        sample_data['name'] = locale_data[name_key]['message']
        sample_data['description'] = locale_data[description_key]['message']
        sample_data['id'] = get_sample_id(sample_data['name'])
      except Exception:
        logging.error(traceback.format_exc())
        # Revert the sample to the original dict.
        sample_data = dict_
      return_list.append(sample_data)
    else:
      dict_['id'] = get_sample_id(name)
      return_list.append(dict_)
  return return_list


def GetEventByNameFromEvents(events):
  '''Parses the dictionary |events| to find the definitions of members of the
  type Event.  Returns a dictionary mapping the name of a member to that
  member's definition.
  '''
  assert 'types' in events, \
      'The dictionary |events| must contain the key "types".'
  event_list = [t for t in events['types'] if t.get('name') == 'Event']
  assert len(event_list) == 1, 'Exactly one type must be called "Event".'
  return _GetByNameDict(event_list[0])


def _GetByNameDict(namespace):
  '''Returns a dictionary mapping names to named items from |namespace|.

  This lets us render specific API entities rather than the whole thing at once,
  for example {{apis.manifestTypes.byName.ExternallyConnectable}}.

  Includes items from namespace['types'], namespace['functions'],
  namespace['events'], and namespace['properties'].
  '''
  by_name = {}
  for item_type in GetNodeCategories():
    if item_type in namespace:
      old_size = len(by_name)
      by_name.update(
          (item['name'], item) for item in namespace[item_type])
      assert len(by_name) == old_size + len(namespace[item_type]), (
          'Duplicate name in %r' % namespace)
  return by_name


def _CreateId(node, prefix):
  if node.parent is not None and not isinstance(node.parent, model.Namespace):
    return '-'.join([prefix, node.parent.simple_name, node.simple_name])
  return '-'.join([prefix, node.simple_name])


def _FormatValue(value):
  '''Inserts commas every three digits for integer values. It is magic.
  '''
  s = str(value)
  return ','.join([s[max(0, i - 3):i] for i in range(len(s), 0, -3)][::-1])


class _JSCViewBuilder(object):
  '''Uses a Model from the JSON Schema Compiler and generates a dict that
  a Motemplate template can use for a data source.
  '''

  def __init__(self,
               content_script_apis,
               jsc_model,
               availability_finder,
               json_cache,
               template_cache,
               features_bundle,
               event_byname_future,
               platform,
               samples):
    self._content_script_apis = content_script_apis
    self._availability = availability_finder.GetAPIAvailability(jsc_model.name)
    self._current_node = APINodeCursor(availability_finder, jsc_model.name)
    self._api_availabilities = json_cache.GetFromFile(
        posixpath.join(JSON_TEMPLATES, 'api_availabilities.json'))
    self._intro_tables = json_cache.GetFromFile(
        posixpath.join(JSON_TEMPLATES, 'intro_tables.json'))
    self._api_features = features_bundle.GetAPIFeatures()
    self._template_cache = template_cache
    self._event_byname_future = event_byname_future
    self._jsc_model = jsc_model
    self._platform = platform
    self._samples = samples

  def _GetLink(self, link):
    ref = link if '.' in link else (self._jsc_model.name + '.' + link)
    return { 'ref': ref, 'text': link, 'name': link }

  def ToDict(self, request):
    '''Returns a dictionary representation of |self._jsc_model|, which
    is a Namespace object from JSON Schema Compiler.
    '''
    assert self._jsc_model is not None
    chrome_dot_name = 'chrome.%s' % self._jsc_model.name
    as_dict = {
      'channelWarning': self._GetChannelWarning(),
      'documentationOptions': self._jsc_model.documentation_options,
      'domEvents': self._GenerateDomEvents(self._jsc_model.events),
      'events': self._GenerateEvents(self._jsc_model.events),
      'functions': self._GenerateFunctions(self._jsc_model.functions),
      'introList': self._GetIntroTableList(),
      'name': self._jsc_model.name,
      'namespace': self._jsc_model.documentation_options.get('namespace',
                                                             chrome_dot_name),
      'properties': self._GenerateProperties(self._jsc_model.properties),
      'samples': CreateSamplesView(self._samples, request),
      'title': self._jsc_model.documentation_options.get('title',
                                                         chrome_dot_name),
      'types': self._GenerateTypes(self._jsc_model.types.values()),
    }
    if self._jsc_model.deprecated:
      as_dict['deprecated'] = self._jsc_model.deprecated

    as_dict['byName'] = _GetByNameDict(as_dict)

    return as_dict

  def _IsExperimental(self):
    return self._jsc_model.name.startswith('experimental')

  def _GetChannelWarning(self):
    if not self._IsExperimental():
      return {
        self._availability.channel_info.channel: True
      }
    return None

  def _GenerateCallback(self, returns_async):
    '''Returns a dictionary representation of a callback suitable
    for consumption by templates.
    '''
    if not returns_async:
      return None
    callback_dict = {
      'name': returns_async.simple_name,
      'simple_type': {'simple_type': 'function'},
      'optional': returns_async.optional,
      'parameters': []
    }
    with self._current_node.Descend('parameters', returns_async.simple_name,
                                    'parameters'):
      for i, param in enumerate(returns_async.params):
        # HACK(https://crbug.com/996488): Callbacks to callbacks of events
        # break, and have historically just been omitted completely due to not
        # checking the 'callback' property here. With the move to ReturnsAsync,
        # the callbacks are now included in the parameters, but this breaks
        # assumptions elsewhere (at the very least, in api_schema_graph.py,
        # which assumes the path will not be this long when looking up the
        # event). For now, hack in ignoring the callback, as we've always done.
        is_callback_to_callback = (
            i == len(returns_async.params) - 1 and
            param.type_.property_type == model.PropertyType.FUNCTION)
        if (is_callback_to_callback):
          continue
        callback_dict['parameters'].append(self._GenerateProperty(param))
    if (len(callback_dict['parameters']) > 0):
      callback_dict['parameters'][-1]['last'] = True
    return callback_dict

  def _GenerateCallbackProperty(self, callback, callback_dict):
    '''Returns a dictionary representation of a callback property
    suitable for consumption by templates.
    '''
    property_dict = {
      'name': callback.simple_name,
      'description': callback.description,
      'optional': callback.optional,
      'isCallback': True,
      'asFunction': callback_dict,
      'id': _CreateId(callback, 'property'),
      'simple_type': 'function',
    }
    if (callback.parent is not None and
        not isinstance(callback.parent, model.Namespace)):
      property_dict['parentName'] = callback.parent.simple_name
    return property_dict

  def _GenerateTypes(self, types):
    '''Returns a list of dictionaries representing this Model's types.
    '''
    with self._current_node.Descend('types'):
      return [self._GenerateType(t) for t in types]

  def _GenerateType(self, type_):
    '''Returns a dictionary representation of a type from JSON Schema Compiler.
    '''
    with self._current_node.Descend(type_.simple_name):
      type_dict = {
        'name': type_.simple_name,
        'description': type_.description,
        'properties': self._GenerateProperties(type_.properties),
        'functions': self._GenerateFunctions(type_.functions),
        'events': self._GenerateEvents(type_.events),
        'id': _CreateId(type_, 'type'),
        'availability': self._GetAvailabilityTemplate(
            is_enum=type_.property_type == model.PropertyType.ENUM)
      }
      self._RenderTypeInformation(type_, type_dict)
      return type_dict

  def _GenerateFunctions(self, functions):
    '''Returns a list of dictionaries representing this Model's functions.
    '''
    with self._current_node.Descend('functions'):
      return [self._GenerateFunction(f) for f in functions.values()]

  def _GenerateFunction(self, function):
    '''Returns a dictionary representation of a function from
    JSON Schema Compiler.
    '''
    # When ignoring types, properties must be ignored as well.
    with self._current_node.Descend(function.simple_name,
                                    ignore=('types', 'properties')):
      function_dict = {
        'name': function.simple_name,
        'description': function.description,
        # TODO(https://crbug.com/1143020): Rename this when we're ready to add
        # promise support into the docs. For now, keep these as callbacks (which
        # are also checked at other places in the docserver code).
        'callback': self._GenerateCallback(function.returns_async),
        'parameters': [],
        'returns': None,
        'id': _CreateId(function, 'method'),
        'availability': self._GetAvailabilityTemplate()
      }
      self._AddCommonProperties(function_dict, function)
      if function.returns:
        function_dict['returns'] = self._GenerateType(function.returns)

    with self._current_node.Descend(function.simple_name, 'parameters'):
      for param in function.params:
        function_dict['parameters'].append(self._GenerateProperty(param))
    if function.returns_async is not None:
      # Show the callback as an extra parameter.
      function_dict['parameters'].append(
          self._GenerateCallbackProperty(function.returns_async,
                                         function_dict['callback']))

    if len(function_dict['parameters']) > 0:
      function_dict['parameters'][-1]['last'] = True
    return function_dict

  def _GenerateEvents(self, events):
    '''Returns a list of dictionaries representing this Model's events.
    '''
    with self._current_node.Descend('events'):
      return [self._GenerateEvent(e) for e in events.values()
              if not e.supports_dom]

  def _GenerateDomEvents(self, events):
    '''Returns a list of dictionaries representing this Model's DOM events.
    '''
    with self._current_node.Descend('events'):
      return [self._GenerateEvent(e) for e in events.values()
              if e.supports_dom]

  def _GenerateEvent(self, event):
    '''Returns a dictionary representation of an event from
    JSON Schema Compiler. Note that although events are modeled as functions
    in JSON Schema Compiler, we model them differently for the templates.
    '''
    with self._current_node.Descend(event.simple_name, ignore=('properties',)):
      event_dict = {
        'name': event.simple_name,
        'description': event.description,
        'filters': [self._GenerateProperty(f) for f in event.filters],
        'conditions': [self._GetLink(condition)
                       for condition in event.conditions],
        'actions': [self._GetLink(action) for action in event.actions],
        'supportsRules': event.supports_rules,
        'supportsListeners': event.supports_listeners,
        'properties': [],
        'id': _CreateId(event, 'event'),
        'byName': {},
        'availability': self._GetAvailabilityTemplate()
      }
    self._AddCommonProperties(event_dict, event)
    # Add the Event members to each event in this object.
    if self._event_byname_future:
      event_dict['byName'].update(self._event_byname_future.Get())
    # We need to create the method description for addListener based on the
    # information stored in |event|.
    if event.supports_listeners:
      callback_object = model.Function(parent=event,
                                       name='callback',
                                       json={},
                                       namespace=event.parent,
                                       origin='')
      callback_object.params = event.params
      if event.returns_async:
        callback_object.returns_async = event.returns_async

      with self._current_node.Descend(event.simple_name):
        callback = self._GenerateFunction(callback_object)
      callback_parameter = self._GenerateCallbackProperty(callback_object,
                                                          callback)
      callback_parameter['last'] = True
      event_dict['byName']['addListener'] = {
        'name': 'addListener',
        'callback': callback,
        'parameters': [callback_parameter]
      }
    if event.supports_dom:
      # Treat params as properties of the custom Event object associated with
      # this DOM Event.
      with self._current_node.Descend(event.simple_name,
                                      ignore=('properties',)):
        event_dict['properties'] += [self._GenerateProperty(param)
                                     for param in event.params]
    return event_dict

  def _GenerateProperties(self, properties):
    '''Returns a list of dictionaries representing this Model's properites.
    '''
    with self._current_node.Descend('properties'):
      return [self._GenerateProperty(v) for v in properties.values()]

  def _GenerateProperty(self, property_):
    '''Returns a dictionary representation of a property from
    JSON Schema Compiler.
    '''
    if not hasattr(property_, 'type_'):
      for d in dir(property_):
        if not d.startswith('_'):
          print ('%s -> %s' % (d, getattr(property_, d)))
    type_ = property_.type_

    # Make sure we generate property info for arrays, too.
    # TODO(kalman): what about choices?
    if type_.property_type == model.PropertyType.ARRAY:
      properties = type_.item_type.properties
    else:
      properties = type_.properties

    with self._current_node.Descend(property_.simple_name):
      property_dict = {
        'name': property_.simple_name,
        'optional': property_.optional,
        'description': property_.description,
        'properties': self._GenerateProperties(type_.properties),
        'functions': self._GenerateFunctions(type_.functions),
        'parameters': [],
        'returns': None,
        'id': _CreateId(property_, 'property'),
        'availability': self._GetAvailabilityTemplate()
      }
      self._AddCommonProperties(property_dict, property_)

      if type_.property_type == model.PropertyType.FUNCTION:
        function = type_.function
        with self._current_node.Descend('parameters'):
          for param in function.params:
            property_dict['parameters'].append(self._GenerateProperty(param))
        if function.returns:
          with self._current_node.Descend(ignore=('types', 'properties')):
            property_dict['returns'] = self._GenerateType(function.returns)

    value = property_.value
    if value is not None:
      if isinstance(value, int):
        property_dict['value'] = _FormatValue(value)
      else:
        property_dict['value'] = value
    else:
      self._RenderTypeInformation(type_, property_dict)

    return property_dict

  def _AddCommonProperties(self, target, src):
    if src.deprecated is not None:
      target['deprecated'] = src.deprecated
    if (src.parent is not None and
        not isinstance(src.parent, model.Namespace)):
      target['parentName'] = src.parent.simple_name

  def _RenderTypeInformation(self, type_, dst_dict):
    with self._current_node.Descend(ignore=('types', 'properties')):
      dst_dict['is_object'] = type_.property_type == model.PropertyType.OBJECT
      if type_.property_type == model.PropertyType.CHOICES:
        dst_dict['choices'] = self._GenerateTypes(type_.choices)
        # We keep track of which == last for knowing when to add "or" between
        # choices in templates.
        if len(dst_dict['choices']) > 0:
          dst_dict['choices'][-1]['last'] = True
      elif type_.property_type == model.PropertyType.REF:
        dst_dict['link'] = self._GetLink(type_.ref_type)
      elif type_.property_type == model.PropertyType.ARRAY:
        dst_dict['array'] = self._GenerateType(type_.item_type)
      elif type_.property_type == model.PropertyType.ENUM:
        dst_dict['enum_values'] = [
            {'name': value.name, 'description': value.description}
            for value in type_.enum_values]
        if len(dst_dict['enum_values']) > 0:
          dst_dict['enum_values'][-1]['last'] = True
          dst_dict['enum_values'][0]['first'] = True
      elif type_.instance_of is not None:
        dst_dict['simple_type'] = type_.instance_of
      else:
        dst_dict['simple_type'] = type_.property_type.name

  def _CreateAvailabilityTemplate(self, status, scheduled, version):
    '''Returns an object suitable for use in templates to display availability
    information.
    '''
    return {
      'partial': self._template_cache.GetFromFile(
          '%sintro_tables/%s_message.html' % (PRIVATE_TEMPLATES, status)).Get(),
      'scheduled': scheduled,
      'version': version
    }

  def _GetAvailabilityTemplate(self, is_enum=False):
    '''Gets availability for the current node and returns an appropriate
    template object.
    '''
    # We don't show an availability warning for enums.
    # TODO(devlin): We should also render enums differently, indicating that
    # symbolic constants are available from version 44 onwards.
    if is_enum:
      return None

    # Displaying deprecated status takes precedence over when the API
    # became stable.
    availability_info = self._current_node.GetDeprecated()
    if availability_info is not None:
      status = 'deprecated'
    else:
      availability_info = self._current_node.GetAvailability()
      if availability_info is None:
        return None
      status = availability_info.channel_info.channel
    return self._CreateAvailabilityTemplate(
        status,
        availability_info.scheduled,
        availability_info.channel_info.version)

  def _GetIntroTableList(self):
    '''Create a generic data structure that can be traversed by the templates
    to create an API intro table.
    '''
    intro_rows = [
      self._GetIntroDescriptionRow(),
      self._GetIntroAvailabilityRow()
    ] + self._GetIntroDependencyRows() + self._GetIntroContentScriptRow()

    # Add rows using data from intro_tables.json, overriding any existing rows
    # if they share the same 'title' attribute.
    row_titles = [row['title'] for row in intro_rows]
    for misc_row in self._GetMiscIntroRows():
      if misc_row['title'] in row_titles:
        intro_rows[row_titles.index(misc_row['title'])] = misc_row
      else:
        intro_rows.append(misc_row)

    return intro_rows

  def _GetIntroContentScriptRow(self):
    '''Generates the 'Content Script' row data for an API intro table.
    '''
    content_script_support = self._content_script_apis.get(self._jsc_model.name)
    if content_script_support is None:
      return []
    if content_script_support.restrictedTo:
      content_script_support.restrictedTo.sort(key=itemgetter('node'))
      MarkFirstAndLast(content_script_support.restrictedTo)
    return [{
      'title': 'Content Scripts',
      'content': [{
        'partial': self._template_cache.GetFromFile(
            posixpath.join(PRIVATE_TEMPLATES,
                           'intro_tables',
                           'content_scripts.html')).Get(),
        'contentScriptSupport': content_script_support.__dict__
      }]
    }]

  def _GetIntroDescriptionRow(self):
    ''' Generates the 'Description' row data for an API intro table.
    '''
    return {
      'title': 'Description',
      'content': [
        { 'text': self._jsc_model.description }
      ]
    }

  def _GetIntroAvailabilityRow(self):
    ''' Generates the 'Availability' row data for an API intro table.
    '''
    if self._IsExperimental():
      status = 'experimental'
      scheduled = None
      version = None
    else:
      status = self._availability.channel_info.channel
      scheduled = self._availability.scheduled
      version = self._availability.channel_info.version
    return {
      'title': 'Availability',
      'content': [
        self._CreateAvailabilityTemplate(status, scheduled, version)
      ]
    }

  def _GetIntroDependencyRows(self):
    # Devtools aren't in _api_features. If we're dealing with devtools, bail.
    if 'devtools' in self._jsc_model.name:
      return []

    api_feature = self._api_features.Get().get(self._jsc_model.name)
    if not api_feature:
      logging.error('"%s" not found in _api_features.json' %
                    self._jsc_model.name)
      return []

    permissions_content = []
    manifest_content = []

    def categorize_dependency(dependency):
      def make_code_node(text):
        return { 'class': 'code', 'text': text }

      context, name = dependency.split(':', 1)
      if context == 'permission':
        permissions_content.append(make_code_node('"%s"' % name))
      elif context == 'manifest':
        manifest_content.append(make_code_node('"%s": {...}' % name))
      elif context == 'api':
        transitive_dependencies = (
            self._api_features.Get().get(name, {}).get('dependencies', []))
        for transitive_dependency in transitive_dependencies:
          categorize_dependency(transitive_dependency)
      else:
        logging.error('Unrecognized dependency for %s: %s' %
                      (self._jsc_model.name, context))

    for dependency in api_feature.get('dependencies', ()):
      categorize_dependency(dependency)

    dependency_rows = []
    if permissions_content:
      dependency_rows.append({
        'title': 'Permissions',
        'content': permissions_content
      })
    if manifest_content:
      dependency_rows.append({
        'title': 'Manifest',
        'content': manifest_content
      })
    return dependency_rows

  def _GetMiscIntroRows(self):
    ''' Generates miscellaneous intro table row data, such as 'Permissions',
    'Samples', and 'Learn More', using intro_tables.json.
    '''
    misc_rows = []
    # Look up the API name in intro_tables.json, which is structured
    # similarly to the data structure being created. If the name is found, loop
    # through the attributes and add them to this structure.
    table_info = self._intro_tables.Get().get(self._jsc_model.name)
    if table_info is None:
      return misc_rows

    for category in table_info.iterkeys():
      content = []
      for node in table_info[category]:
        ext_type = PlatformToExtensionType(self._platform)
        # Don't display nodes restricted to a different platform.
        if ext_type not in node.get('extension_types', (ext_type,)):
          continue
        # If there is a 'partial' argument and it hasn't already been
        # converted to a Motemplate object, transform it to a template.
        if 'partial' in node:
          # Note: it's enough to copy() not deepcopy() because only a single
          # top-level key is being modified.
          node = copy(node)
          node['partial'] = self._template_cache.GetFromFile(
              posixpath.join(PRIVATE_TEMPLATES, node['partial'])).Get()
        content.append(node)
      misc_rows.append({ 'title': category, 'content': content })
    return misc_rows

def CreateJSCView(content_script_apis,
                  jsc_model,
                  availability_finder,
                  json_cache,
                  template_cache,
                  features_bundle,
                  event_byname_future,
                  platform,
                  samples,
                  request):
  return _JSCViewBuilder(content_script_apis,
                         jsc_model,
                         availability_finder,
                         json_cache,
                         template_cache,
                         features_bundle,
                         event_byname_future,
                         platform,
                         samples).ToDict(request)

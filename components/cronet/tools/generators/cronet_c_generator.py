# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates C source files from an idl (mojom) Module."""

import os

# This is called from cronet_bindings_generator.py which does some magic to add
# libraries to the lookup path which pylint does not
# pylint: disable=import-error
import mojom.generate.generator as generator
import mojom.generate.module as mojom
import mojom.generate.pack as pack
from mojom.generate.template_expander import UseJinja
# pylint: enable=import-error

# pylint: disable=useless-object-inheritance,super-with-arguments


_kind_to_cpp_type = {
  mojom.BOOL:                  "bool",
  mojom.INT8:                  "int8_t",
  mojom.UINT8:                 "uint8_t",
  mojom.INT16:                 "int16_t",
  mojom.UINT16:                "uint16_t",
  mojom.INT32:                 "int32_t",
  mojom.UINT32:                "uint32_t",
  mojom.FLOAT:                 "float",
  mojom.INT64:                 "int64_t",
  mojom.UINT64:                "uint64_t",
  mojom.DOUBLE:                "double",
}

_kind_to_cpp_literal_suffix = {
  mojom.UINT8:        "U",
  mojom.UINT16:       "U",
  mojom.UINT32:       "U",
  mojom.FLOAT:        "f",
  mojom.UINT64:       "ULL",
}

ATTRIBUTE_ABSTRACT = "Abstract"

class _NameFormatter(object):
  """A formatter for the names of kinds or values."""

  def __init__(self, token, variant):
    self._token = token
    self._variant = variant

  def Format(self, separator, prefixed=False, internal=False,
             include_variant=False, omit_namespace_for_module=None,
             flatten_nested_kind=False):
    """Formats the name according to the given configuration.

    Args:
      separator: Separator between different parts of the name.
      prefixed: Whether a leading separator should be added.
      internal: Returns the name in the "internal" namespace.
      include_variant: Whether to include variant as namespace. If |internal| is
          True, then this flag is ignored and variant is not included.
      omit_namespace_for_module: If the token is from the specified module,
          don't add the namespaces of the module to the name.
      flatten_nested_kind: It is allowed to define enums inside structs and
          interfaces. If this flag is set to True, this method concatenates the
          parent kind and the nested kind with '_', instead of treating the
          parent kind as a scope."""

    parts = []
    if self._ShouldIncludeNamespace(omit_namespace_for_module):
      parts.extend(self._GetNamespace())
      if include_variant and self._variant and not internal:
        parts.append(self._variant)
    parts.extend(self._GetName(internal, flatten_nested_kind))
    return separator.join(parts)

  def FormatForCpp(self, omit_namespace_for_module=None, internal=False,
                   flatten_nested_kind=False):
    return self.Format(
        "_", prefixed=True,
        omit_namespace_for_module=omit_namespace_for_module,
        internal=internal, include_variant=True,
        flatten_nested_kind=flatten_nested_kind)

  def FormatForMojom(self):
    return self.Format(".")

  def _MapKindName(self, token, internal):
    if not internal:
      return token.name
    if (mojom.IsStructKind(token) or mojom.IsUnionKind(token) or
        mojom.IsEnumKind(token)):
      return token.name + "_Data"
    return token.name

  def _GetName(self, internal, flatten_nested_kind):
    if isinstance(self._token, mojom.EnumValue):
      name_parts = _NameFormatter(self._token.enum, self._variant)._GetName(
          internal, flatten_nested_kind)
      name_parts.append(self._token.name)
      return name_parts

    name_parts = []
    if internal:
      name_parts.append("internal")

    if (flatten_nested_kind and mojom.IsEnumKind(self._token) and
        self._token.parent_kind):
      name = "%s_%s" % (self._token.parent_kind.name,
                        self._MapKindName(self._token, internal))
      name_parts.append(name)
      return name_parts

    if self._token.parent_kind:
      name_parts.append(self._MapKindName(self._token.parent_kind, internal))
    name_parts.append(self._MapKindName(self._token, internal))
    return name_parts

  def _ShouldIncludeNamespace(self, omit_namespace_for_module):
    return self._token.module


  # pylint: disable=inconsistent-return-statements
  def _GetNamespace(self):
    if self._token.module:
      return NamespaceToArray(self._token.module.namespace)
  # pylint: enable=inconsistent-return-statements


def NamespaceToArray(namespace):
  return namespace.split(".") if namespace else []


def IsNativeOnlyKind(kind):
  return (mojom.IsStructKind(kind) or mojom.IsEnumKind(kind)) and \
      kind.native_only


def UseCustomSerializer(kind):
  return mojom.IsStructKind(kind) and kind.custom_serializer


def AllEnumValues(enum):
  """Return all enum values associated with an enum.

  Args:
    enum: {mojom.Enum} The enum type.

  Returns:
   {Set[int]} The values.
  """
  return set(field.numeric_value for field in enum.fields)


def GetCppPodType(kind):
  return _kind_to_cpp_type[kind]


def RequiresContextForDataView(kind):
  for field in kind.fields:
    if mojom.IsReferenceKind(field.kind):
      return True
  return False


def ShouldInlineStruct(struct):
  # TODO(darin): Base this on the size of the wrapper class.
  if len(struct.fields) > 4:
    return False
  for field in struct.fields:
    if mojom.IsReferenceKind(field.kind) and not mojom.IsStringKind(field.kind):
      return False
  return True


def ShouldInlineUnion(union):
  return not any(
      mojom.IsReferenceKind(field.kind) and not mojom.IsStringKind(field.kind)
           for field in union.fields)

def IsAbstract(kind):
  return kind.attributes.get(ATTRIBUTE_ABSTRACT, False) \
      if kind.attributes else False

class StructConstructor(object):
  """Represents a constructor for a generated struct.

  Fields:
    fields: {[Field]} All struct fields in order.
    params: {[Field]} The fields that are passed as params.
  """

  def __init__(self, fields, params):
    self._fields = fields
    self._params = set(params)

  @property
  def params(self):
    return [field for field in self._fields if field in self._params]

  @property
  def fields(self):
    for field in self._fields:
      yield (field, field in self._params)


class Generator(generator.Generator):

  def _GetExtraTraitsHeaders(self):
    extra_headers = set()
    for typemap in self._GetAllUsedTypemaps():
      extra_headers.update(typemap.get("traits_headers", []))
    return sorted(extra_headers)

  def _GetAllUsedTypemaps(self):
    """Returns the typemaps for types needed for serialization in this module.

    A type is needed for serialization if it is contained by a struct or union
    defined in this module, is a parameter of a message in an interface in
    this module or is contained within another type needed for serialization.
    """
    used_typemaps = []
    seen_types = set()
    def IsBasicKind(kind):
      return (mojom.IsIntegralKind(kind) or mojom.IsStringKind(kind) or
              mojom.IsDoubleKind(kind) or mojom.IsFloatKind(kind) or
              mojom.IsAnyHandleKind(kind) or
              mojom.IsInterfaceKind(kind) or
              mojom.IsInterfaceRequestKind(kind) or
              mojom.IsAssociatedKind(kind))

    def AddKind(kind):
      if IsBasicKind(kind):
        pass
      elif mojom.IsArrayKind(kind):
        AddKind(kind.kind)
      elif mojom.IsMapKind(kind):
        AddKind(kind.key_kind)
        AddKind(kind.value_kind)
      else:
        name = self._GetFullMojomNameForKind(kind)
        if name in seen_types:
          return
        seen_types.add(name)

        typemap = self.typemap.get(name, None)
        if typemap:
          used_typemaps.append(typemap)
        if mojom.IsStructKind(kind) or mojom.IsUnionKind(kind):
          for field in kind.fields:
            AddKind(field.kind)

    for kind in self.module.structs + self.module.unions:
      for field in kind.fields:
        AddKind(field.kind)

    for interface in self.module.interfaces:
      for method in interface.methods:
        for parameter in method.parameters + (method.response_parameters or []):
          AddKind(parameter.kind)

    return used_typemaps

  def _GetExtraPublicHeaders(self):
    all_enums = list(self.module.enums)
    for struct in self.module.structs:
      all_enums.extend(struct.enums)
    for interface in self.module.interfaces:
      all_enums.extend(interface.enums)

    types = set(self._GetFullMojomNameForKind(typename)
                for typename in
                self.module.structs + all_enums + self.module.unions)
    headers = set()
    for typename, typemap in self.typemap.items():
      if typename in types:
        headers.update(typemap.get("public_headers", []))
    return sorted(headers)

  def _GetDirectlyUsedKinds(self):
    for struct in self.module.structs + self.module.unions:
      for field in struct.fields:
        yield field.kind

    for interface in self.module.interfaces:
      for method in interface.methods:
        for param in method.parameters + (method.response_parameters or []):
          yield param.kind

  def _GetJinjaExports(self):
    all_enums = list(self.module.enums)
    for struct in self.module.structs:
      all_enums.extend(struct.enums)
    for interface in self.module.interfaces:
      all_enums.extend(interface.enums)

    return {
      "all_enums": all_enums,
      "enums": self.module.enums,
      "export_attribute": self.export_attribute,
      "export_header": self.export_header,
      "extra_public_headers": self._GetExtraPublicHeaders(),
      "extra_traits_headers": self._GetExtraTraitsHeaders(),
      "for_blink": self.for_blink,
      "imports": self.module.imports,
      "interfaces": self.module.interfaces,
      "kinds": self.module.kinds,
      "module": self.module,
      "namespace": self.module.namespace,
      "namespaces_as_array": NamespaceToArray(self.module.namespace),
      "structs": self.module.structs,
      "unions": self.module.unions,
      "variant": self.variant,
    }

  @staticmethod
  def GetTemplatePrefix():
    return "c_templates"

  def GetFilters(self):
    cpp_filters = {
      "all_enum_values": AllEnumValues,
      "c_wrapper_type": self._GetCWrapperType,
      "constant_value": self._ConstantValue,
      "contains_handles_or_interfaces": mojom.ContainsHandlesOrInterfaces,
      "contains_move_only_members": self._ContainsMoveOnlyMembers,
      "cpp_wrapper_param_type": self._GetCppWrapperParamType,
      "cpp_data_view_type": self._GetCppDataViewType,
      "cpp_field_type": self._GetCppFieldType,
      "cpp_union_field_type": self._GetCppUnionFieldType,
      "cpp_pod_type": GetCppPodType,
      "cpp_union_getter_return_type": self._GetUnionGetterReturnType,
      "cpp_union_trait_getter_return_type": self._GetUnionTraitGetterReturnType,
      "cpp_wrapper_type": self._GetCppWrapperType,
      "default_value": self._DefaultValue,
      "expression_to_text": self._ExpressionToText,
      "format_constant_declaration": self._FormatConstantDeclaration,
      "get_container_validate_params_ctor_args":
          self._GetContainerValidateParamsCtorArgs,
      "get_name_for_kind": self._GetNameForKind,
      "get_pad": pack.GetPad,
      "get_qualified_name_for_kind": self._GetQualifiedNameForKind,
      "has_callbacks": mojom.HasCallbacks,
      "has_sync_methods": mojom.HasSyncMethods,
      "requires_context_for_data_view": RequiresContextForDataView,
      "should_inline": ShouldInlineStruct,
      "should_inline_union": ShouldInlineUnion,
      "is_abstract": IsAbstract,
      "is_array_kind": mojom.IsArrayKind,
      "is_enum_kind": mojom.IsEnumKind,
      "is_integral_kind": mojom.IsIntegralKind,
      "is_native_only_kind": IsNativeOnlyKind,
      "is_any_handle_kind": mojom.IsAnyHandleKind,
      "is_any_interface_kind": mojom.IsAnyInterfaceKind,
      "is_any_handle_or_interface_kind": mojom.IsAnyHandleOrInterfaceKind,
      "is_associated_kind": mojom.IsAssociatedKind,
      "is_hashable": self._IsHashableKind,
      "is_map_kind": mojom.IsMapKind,
      "is_nullable_kind": mojom.IsNullableKind,
      "is_object_kind": mojom.IsObjectKind,
      "is_reference_kind": mojom.IsReferenceKind,
      "is_string_kind": mojom.IsStringKind,
      "is_struct_kind": mojom.IsStructKind,
      "is_typemapped_kind": self._IsTypemappedKind,
      "is_union_kind": mojom.IsUnionKind,
      "passes_associated_kinds": mojom.PassesAssociatedKinds,
      "struct_constructors": self._GetStructConstructors,
      "under_to_camel": generator.ToCamel,
      "unmapped_type_for_serializer": self._GetUnmappedTypeForSerializer,
    }
    return cpp_filters

  @UseJinja("module_c.h.tmpl")
  def _GenerateModuleHeader(self):
    return self._GetJinjaExports()

  @UseJinja("module_impl_interface.h.tmpl")
  def _GenerateModuleInterfaceHeader(self):
    return self._GetJinjaExports()

  @UseJinja("module_impl_interface.cc.tmpl")
  def _GenerateModuleInterfaceSource(self):
    return self._GetJinjaExports()

  @UseJinja("module_impl_interface_unittest.cc.tmpl")
  def _GenerateModuleInterfaceUnittest(self):
    return self._GetJinjaExports()

  @UseJinja("module_impl_struct.h.tmpl")
  def _GenerateModuleStructHeader(self):
    return self._GetJinjaExports()

  @UseJinja("module_impl_struct.cc.tmpl")
  def _GenerateModuleStructSource(self):
    return self._GetJinjaExports()

  @UseJinja("module_impl_struct_unittest.cc.tmpl")
  def _GenerateModuleStructUnittest(self):
    return self._GetJinjaExports()

  @UseJinja("module.cc.tmpl")
  def _GenerateModuleSource(self):
    return self._GetJinjaExports()

  @UseJinja("module-shared.h.tmpl")
  def _GenerateModuleSharedHeader(self):
    return self._GetJinjaExports()

  @UseJinja("module-shared-internal.h.tmpl")
  def _GenerateModuleSharedInternalHeader(self):
    return self._GetJinjaExports()

  @UseJinja("module-shared.cc.tmpl")
  def _GenerateModuleSharedSource(self):
    return self._GetJinjaExports()

  def GenerateFiles(self, args):
    self.module.Stylize(generator.Stylizer())
    # TODO(mef): Remove this when generated files are not checked in.
    path, module = os.path.split(self.module.path)
    self.module.path = os.path.join(path, "generated", module)

    if self.generate_non_variant_code:
      self.Write(self._GenerateModuleSharedHeader(),
                 "%s-shared.h" % self.module.path)
      self.Write(self._GenerateModuleSharedInternalHeader(),
                 "%s-shared-internal.h" % self.module.path)
      self.Write(self._GenerateModuleSharedSource(),
                 "%s-shared.cc" % self.module.path)
    else:
      suffix = "-%s" % self.variant if self.variant else ""
      self.Write(self._GenerateModuleHeader(),
                 "%s%s_c.h" % (self.module.path, suffix))
      self.Write(self._GenerateModuleInterfaceHeader(),
                 "%s%s_impl_interface.h" % (self.module.path, suffix))
      self.Write(self._GenerateModuleInterfaceSource(),
                 "%s%s_impl_interface.cc" % (self.module.path, suffix))
      self.Write(self._GenerateModuleInterfaceUnittest(),
                 "%s%s_impl_interface_unittest.cc" % (self.module.path, suffix))
      self.Write(self._GenerateModuleStructHeader(),
                 "%s%s_impl_struct.h" % (self.module.path, suffix))
      self.Write(self._GenerateModuleStructSource(),
                 "%s%s_impl_struct.cc" % (self.module.path, suffix))
      self.Write(self._GenerateModuleStructUnittest(),
                 "%s%s_impl_struct_unittest.cc" % (self.module.path, suffix))

  def _ConstantValue(self, constant):
    return self._ExpressionToText(constant.value, kind=constant.kind)

  def _DefaultValue(self, field):
    if not field.default:
      return ""

    if mojom.IsStructKind(field.kind):
      assert field.default == "default"
      if self._IsTypemappedKind(field.kind):
        return ""
      return "%s::New()" % self._GetNameForKind(field.kind)

    expression = self._ExpressionToText(field.default, kind=field.kind)
    if mojom.IsEnumKind(field.kind) and self._IsTypemappedKind(field.kind):
      expression = "mojo::internal::ConvertEnumValue<%s, %s>(%s)" % (
          self._GetNameForKind(field.kind), self._GetCppWrapperType(field.kind),
          expression)
    return expression

  def _GetNameForKind(self, kind, internal=False, flatten_nested_kind=False,
                      add_same_module_namespaces=False):
    return _NameFormatter(kind, self.variant).FormatForCpp(
        internal=internal, flatten_nested_kind=flatten_nested_kind,
        omit_namespace_for_module = (None if add_same_module_namespaces
                                          else self.module))

  def _GetQualifiedNameForKind(self, kind, internal=False,
                               flatten_nested_kind=False, include_variant=True):
    return _NameFormatter(
        kind, self.variant if include_variant else None).FormatForCpp(
            internal=internal, flatten_nested_kind=flatten_nested_kind)

  def _GetFullMojomNameForKind(self, kind):
    return _NameFormatter(kind, self.variant).FormatForMojom()

  def _IsTypemappedKind(self, kind):
    return hasattr(kind, "name") and \
        self._GetFullMojomNameForKind(kind) in self.typemap

  def _IsHashableKind(self, kind):
    """Check if the kind can be hashed.

    Args:
      kind: {Kind} The kind to check.

    Returns:
      {bool} True if a value of this kind can be hashed.
    """
    checked = set()
    def Check(kind):
      if kind.spec in checked:
        return True
      checked.add(kind.spec)
      if mojom.IsNullableKind(kind):
        return False
      if mojom.IsStructKind(kind):
        if kind.native_only:
          return False
        if (self._IsTypemappedKind(kind) and
            not self.typemap[self._GetFullMojomNameForKind(kind)]["hashable"]):
          return False
        return all(Check(field.kind) for field in kind.fields)
      if mojom.IsEnumKind(kind):
        return not self._IsTypemappedKind(kind) or self.typemap[
            self._GetFullMojomNameForKind(kind)]["hashable"]
      if mojom.IsUnionKind(kind):
        return all(Check(field.kind) for field in kind.fields)
      if mojom.IsAnyHandleKind(kind):
        return False
      if mojom.IsAnyInterfaceKind(kind):
        return False
      # TODO(crbug.com/41326458): Arrays and maps could be made hashable. We just
      # don't have a use case yet.
      if mojom.IsArrayKind(kind):
        return False
      if mojom.IsMapKind(kind):
        return False
      return True
    return Check(kind)

  def _GetNativeTypeName(self, typemapped_kind):
    return self.typemap[self._GetFullMojomNameForKind(typemapped_kind)][
        "typename"]

  def _FormatConstantDeclaration(self, constant, nested=False):
    if mojom.IsStringKind(constant.kind):
      if nested:
        return "const char %s[]" % constant.name
      return "%sextern const char %s[]" % \
          ((self.export_attribute + " ") if self.export_attribute else "",
           constant.name)
    return "const %s %s_%s = %s" % (
        GetCppPodType(constant.kind), self.module.namespace, constant.name,
        self._ConstantValue(constant))

  def _GetCppWrapperType(self, kind, add_same_module_namespaces=False):
    def _AddOptional(type_name):
      return "std::optional<%s>" % type_name

    if self._IsTypemappedKind(kind):
      type_name = self._GetNativeTypeName(kind)
      if (mojom.IsNullableKind(kind) and
          not self.typemap[self._GetFullMojomNameForKind(kind)][
             "nullable_is_same_type"]):
        type_name = _AddOptional(type_name)
      return type_name
    if mojom.IsEnumKind(kind):
      return self._GetNameForKind(
          kind, add_same_module_namespaces=add_same_module_namespaces)
    if mojom.IsStructKind(kind) or mojom.IsUnionKind(kind):
      return "%s" % self._GetNameForKind(
          kind, add_same_module_namespaces=add_same_module_namespaces)
    if mojom.IsArrayKind(kind):
      pattern = "WTF::Vector<%s>" if self.for_blink else "std::vector<%s>"
      if mojom.IsNullableKind(kind):
        pattern = _AddOptional(pattern)
      return pattern % self._GetCppWrapperType(
          kind.kind, add_same_module_namespaces=add_same_module_namespaces)
    if mojom.IsMapKind(kind):
      pattern = ("WTF::HashMap<%s, %s>" if self.for_blink else
                 "std::unordered_map<%s, %s>")
      if mojom.IsNullableKind(kind):
        pattern = _AddOptional(pattern)
      return pattern % (
          self._GetCppWrapperType(
              kind.key_kind,
              add_same_module_namespaces=add_same_module_namespaces),
          self._GetCppWrapperType(
              kind.value_kind,
              add_same_module_namespaces=add_same_module_namespaces))
    if mojom.IsInterfaceKind(kind):
      return "%sPtr" % self._GetNameForKind(
          kind, add_same_module_namespaces=add_same_module_namespaces)
    if mojom.IsInterfaceRequestKind(kind):
      return "%sRequest" % self._GetNameForKind(
          kind.kind, add_same_module_namespaces=add_same_module_namespaces)
    if mojom.IsAssociatedInterfaceKind(kind):
      return "%sAssociatedPtrInfo" % self._GetNameForKind(
          kind.kind, add_same_module_namespaces=add_same_module_namespaces)
    if mojom.IsAssociatedInterfaceRequestKind(kind):
      return "%sAssociatedRequest" % self._GetNameForKind(
          kind.kind, add_same_module_namespaces=add_same_module_namespaces)
    if mojom.IsStringKind(kind):
      if self.for_blink:
        return "WTF::String"
      type_name = "std::string"
      return (_AddOptional(type_name) if mojom.IsNullableKind(kind)
                                      else type_name)
    if mojom.IsGenericHandleKind(kind):
      return "Cronet_RawDataPtr"
    if mojom.IsDataPipeConsumerKind(kind):
      return "mojo::ScopedDataPipeConsumerHandle"
    if mojom.IsDataPipeProducerKind(kind):
      return "mojo::ScopedDataPipeProducerHandle"
    if mojom.IsMessagePipeKind(kind):
      return "mojo::ScopedMessagePipeHandle"
    if mojom.IsSharedBufferKind(kind):
      return "mojo::ScopedSharedBufferHandle"
    if not kind in _kind_to_cpp_type:
      raise Exception("Unrecognized kind %s" % kind.spec)
    return _kind_to_cpp_type[kind]

  def _IsMoveOnlyKind(self, kind):
    if self._IsTypemappedKind(kind):
      if mojom.IsEnumKind(kind):
        return False
      return self.typemap[self._GetFullMojomNameForKind(kind)]["move_only"]
    if mojom.IsStructKind(kind) or mojom.IsUnionKind(kind):
      return True
    if mojom.IsArrayKind(kind):
      return self._IsMoveOnlyKind(kind.kind)
    if mojom.IsMapKind(kind):
      return (self._IsMoveOnlyKind(kind.value_kind) or
              self._IsMoveOnlyKind(kind.key_kind))
    if mojom.IsAnyHandleOrInterfaceKind(kind):
      return True
    return False

  def _IsCopyablePassByValue(self, kind):
    if not self._IsTypemappedKind(kind):
      return False
    return self.typemap[self._GetFullMojomNameForKind(kind)][
        "copyable_pass_by_value"]

  def _ShouldPassParamByValue(self, kind):
    return ((not mojom.IsReferenceKind(kind)) or self._IsMoveOnlyKind(kind) or
        self._IsCopyablePassByValue(kind))

  def _GetCWrapperType(self, kind):
    if mojom.IsStringKind(kind):
      return "Cronet_String"
    if mojom.IsStructKind(kind) or mojom.IsUnionKind(kind):
      return "%sPtr" % self._GetNameForKind(kind)
    return self._GetCppWrapperType(kind)

  def _GetCppWrapperParamType(self, kind):
    cpp_wrapper_type = self._GetCppWrapperType(kind)
    return (cpp_wrapper_type if self._ShouldPassParamByValue(kind)
                             else "const %s&" % cpp_wrapper_type)

  def _GetCppFieldType(self, kind):
    if mojom.IsStructKind(kind):
      return ("mojo::internal::Pointer<%s>" %
          self._GetNameForKind(kind, internal=True))
    if mojom.IsUnionKind(kind):
      return "%s" % self._GetNameForKind(kind, internal=True)
    if mojom.IsArrayKind(kind):
      return ("mojo::internal::Pointer<mojo::internal::Array_Data<%s>>" %
              self._GetCppFieldType(kind.kind))
    if mojom.IsMapKind(kind):
      return ("mojo::internal::Pointer<mojo::internal::Map_Data<%s, %s>>" %
              (self._GetCppFieldType(kind.key_kind),
               self._GetCppFieldType(kind.value_kind)))
    if mojom.IsInterfaceKind(kind):
      return "mojo::internal::Interface_Data"
    if mojom.IsInterfaceRequestKind(kind):
      return "mojo::internal::Handle_Data"
    if mojom.IsAssociatedInterfaceKind(kind):
      return "mojo::internal::AssociatedInterface_Data"
    if mojom.IsAssociatedInterfaceRequestKind(kind):
      return "mojo::internal::AssociatedEndpointHandle_Data"
    if mojom.IsEnumKind(kind):
      return "int32_t"
    if mojom.IsStringKind(kind):
      return "mojo::internal::Pointer<mojo::internal::String_Data>"
    if mojom.IsAnyHandleKind(kind):
      return "mojo::internal::Handle_Data"
    return _kind_to_cpp_type[kind]

  def _GetCppUnionFieldType(self, kind):
    if mojom.IsUnionKind(kind):
      return ("mojo::internal::Pointer<%s>" %
                  self._GetNameForKind(kind, internal=True))
    return self._GetCppFieldType(kind)

  def _GetUnionGetterReturnType(self, kind):
    if mojom.IsReferenceKind(kind):
      return "%s&" % self._GetCppWrapperType(kind)
    return self._GetCppWrapperType(kind)

  def _GetUnionTraitGetterReturnType(self, kind):
    """Get field type used in UnionTraits template specialization.

    The type may be qualified as UnionTraits specializations live outside the
    namespace where e.g. structs are defined.

    Args:
      kind: {Kind} The type of the field.

    Returns:
      {str} The C++ type to use for the field.
    """
    if mojom.IsReferenceKind(kind):
      return "%s&" % self._GetCppWrapperType(kind,
                                             add_same_module_namespaces=True)
    return self._GetCppWrapperType(kind, add_same_module_namespaces=True)

  def _TranslateConstants(self, token, kind):
    if isinstance(token, mojom.NamedValue):
      return self._GetNameForKind(token, flatten_nested_kind=True)

    if isinstance(token, mojom.BuiltinValue):
      if token.value == "double.INFINITY":
        return "std::numeric_limits<double>::infinity()"
      if token.value == "float.INFINITY":
        return "std::numeric_limits<float>::infinity()"
      if token.value == "double.NEGATIVE_INFINITY":
        return "-std::numeric_limits<double>::infinity()"
      if token.value == "float.NEGATIVE_INFINITY":
        return "-std::numeric_limits<float>::infinity()"
      if token.value == "double.NAN":
        return "std::numeric_limits<double>::quiet_NaN()"
      if token.value == "float.NAN":
        return "std::numeric_limits<float>::quiet_NaN()"

    if (kind is not None and mojom.IsFloatKind(kind)):
      return token if token.isdigit() else token + "f";

    return "%s%s" % (token, _kind_to_cpp_literal_suffix.get(kind, ""))

  def _ExpressionToText(self, value, kind=None):
    return self._TranslateConstants(value, kind)

  def _ContainsMoveOnlyMembers(self, struct):
    for field in struct.fields:
      if self._IsMoveOnlyKind(field.kind):
        return True
    return False

  def _GetStructConstructors(self, struct):
    """Returns a list of constructors for a struct.

    Params:
      struct: {Struct} The struct to return constructors for.

    Returns:
      {[StructConstructor]} A list of StructConstructors that should be
      generated for |struct|.
    """
    if not mojom.IsStructKind(struct):
      raise TypeError
    # Types that are neither copyable nor movable can't be passed to a struct
    # constructor so only generate a default constructor.
    if any(self._IsTypemappedKind(field.kind) and self.typemap[
        self._GetFullMojomNameForKind(field.kind)]["non_copyable_non_movable"]
           for field in struct.fields):
      return [StructConstructor(struct.fields, [])]

    param_counts = [0]
    for version in struct.versions:
      if param_counts[-1] != version.num_fields:
        param_counts.append(version.num_fields)

    ordinal_fields = sorted(struct.fields, key=lambda field: field.ordinal)
    return (StructConstructor(struct.fields, ordinal_fields[:param_count])
            for param_count in param_counts)

  def _GetContainerValidateParamsCtorArgs(self, kind):
    if mojom.IsStringKind(kind):
      expected_num_elements = 0
      element_is_nullable = False
      key_validate_params = "nullptr"
      element_validate_params = "nullptr"
      enum_validate_func = "nullptr"
    elif mojom.IsMapKind(kind):
      expected_num_elements = 0
      element_is_nullable = False
      key_validate_params = self._GetNewContainerValidateParams(mojom.Array(
          kind=kind.key_kind))
      element_validate_params = self._GetNewContainerValidateParams(mojom.Array(
          kind=kind.value_kind))
      enum_validate_func = "nullptr"
    else:  # mojom.IsArrayKind(kind)
      expected_num_elements = generator.ExpectedArraySize(kind) or 0
      element_is_nullable = mojom.IsNullableKind(kind.kind)
      key_validate_params = "nullptr"
      element_validate_params = self._GetNewContainerValidateParams(kind.kind)
      if mojom.IsEnumKind(kind.kind):
        enum_validate_func = ("%s::Validate" %
            self._GetQualifiedNameForKind(kind.kind, internal=True,
                                          flatten_nested_kind=True))
      else:
        enum_validate_func = "nullptr"

    if enum_validate_func == "nullptr":
      if key_validate_params == "nullptr":
        return "%d, %s, %s" % (expected_num_elements,
                               "true" if element_is_nullable else "false",
                               element_validate_params)
      return "%s, %s" % (key_validate_params, element_validate_params)
    return "%d, %s" % (expected_num_elements, enum_validate_func)

  def _GetNewContainerValidateParams(self, kind):
    if (not mojom.IsArrayKind(kind) and not mojom.IsMapKind(kind) and
        not mojom.IsStringKind(kind)):
      return "nullptr"

    return "new mojo::internal::ContainerValidateParams(%s)" % (
        self._GetContainerValidateParamsCtorArgs(kind))

  def _GetCppDataViewType(self, kind, qualified=False):
    def _GetName(input_kind):
      return _NameFormatter(input_kind, None).FormatForCpp(
          omit_namespace_for_module=(None if qualified else self.module),
          flatten_nested_kind=True)

    if mojom.IsEnumKind(kind):
      return _GetName(kind)
    if mojom.IsStructKind(kind) or mojom.IsUnionKind(kind):
      return "%sDataView" % _GetName(kind)
    if mojom.IsArrayKind(kind):
      return "mojo::ArrayDataView<%s>" % (
          self._GetCppDataViewType(kind.kind, qualified))
    if mojom.IsMapKind(kind):
      return ("mojo::MapDataView<%s, %s>" % (
          self._GetCppDataViewType(kind.key_kind, qualified),
          self._GetCppDataViewType(kind.value_kind, qualified)))
    if mojom.IsStringKind(kind):
      return "mojo::StringDataView"
    if mojom.IsInterfaceKind(kind):
      return "%sPtrDataView" % _GetName(kind)
    if mojom.IsInterfaceRequestKind(kind):
      return "%sRequestDataView" % _GetName(kind.kind)
    if mojom.IsAssociatedInterfaceKind(kind):
      return "%sAssociatedPtrInfoDataView" % _GetName(kind.kind)
    if mojom.IsAssociatedInterfaceRequestKind(kind):
      return "%sAssociatedRequestDataView" % _GetName(kind.kind)
    if mojom.IsGenericHandleKind(kind):
      return "mojo::ScopedHandle"
    if mojom.IsDataPipeConsumerKind(kind):
      return "mojo::ScopedDataPipeConsumerHandle"
    if mojom.IsDataPipeProducerKind(kind):
      return "mojo::ScopedDataPipeProducerHandle"
    if mojom.IsMessagePipeKind(kind):
      return "mojo::ScopedMessagePipeHandle"
    if mojom.IsSharedBufferKind(kind):
      return "mojo::ScopedSharedBufferHandle"
    return _kind_to_cpp_type[kind]

  def _GetUnmappedTypeForSerializer(self, kind):
    return self._GetCppDataViewType(kind, qualified=True)

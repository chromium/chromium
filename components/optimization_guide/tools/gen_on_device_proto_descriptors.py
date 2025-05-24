#!/usr/bin/env python
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Code generator for proto descriptors used for on-device model execution.

This script generates a C++ source file containing the proto descriptors.

To check the type annotations in this file, install mypy then:

autoninja -C $OUT_DIR protoc
$OUT_DIR/protoc -I=../../../third_party/protobuf/src \
    --pyi_out=. \
    ../../../third_party/protobuf/src/google/protobuf/descriptor.proto
mypy gen_on_device_proto_descriptors.py
"""
from __future__ import annotations

from collections.abc import Iterator
import dataclasses
import functools
from io import StringIO
import optparse
import os
import collections
import re
import sys
from typing import IO

_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..'))
sys.path.insert(0, os.path.join(_SRC_PATH, 'third_party', 'protobuf',
                                'python'))

from google.protobuf import descriptor_pb2


class Error(Exception):
    pass


class Type:
    """Aliases for FieldDescriptorProto::Type(s)."""
    DOUBLE = 1
    FLOAT = 2
    INT64 = 3
    UINT64 = 4
    INT32 = 5
    FIXED64 = 6
    FIXED32 = 7
    BOOL = 8
    STRING = 9
    GROUP = 10
    MESSAGE = 11
    BYTES = 12
    UINT32 = 13
    ENUM = 14
    SFIXED32 = 15
    SFIXED64 = 16
    SINT32 = 17
    SINT64 = 18


@dataclasses.dataclass(frozen=True)
class BaseValueType:
    cpptype: str
    getIfFn: str


class VType:
    """Base::Value types."""
    DOUBLE = BaseValueType("std::optional<double>", "Double")
    BOOL = BaseValueType("std::optional<bool>", "Bool")
    INT = BaseValueType("std::optional<int>", "Int")
    STRING = BaseValueType("std::string*", "String")
    BLOB = BaseValueType("BlobStorage*", "Blob")
    DICT = BaseValueType("Dict*", "Dict")
    LIST = BaseValueType("List*", "List")


BASE_VALUE_TYPES = {
    Type.DOUBLE: VType.DOUBLE,
    Type.FLOAT: VType.DOUBLE,
    Type.INT64: VType.INT,
    Type.UINT64: VType.INT,
    Type.INT32: VType.INT,
    Type.FIXED64: VType.INT,
    Type.FIXED32: VType.INT,
    Type.BOOL: VType.BOOL,
    Type.STRING: VType.STRING,
    Type.GROUP: VType.STRING,  # Not handled
    Type.MESSAGE: VType.DICT,  # Not handled
    Type.BYTES: VType.STRING,  # Not handled
    Type.UINT32: VType.INT,
    Type.ENUM: VType.INT,  # Not handled
    Type.SFIXED32: VType.INT,
    Type.SFIXED64: VType.INT,
    Type.SINT32: VType.INT,
    Type.SINT64: VType.INT,
}


@dataclasses.dataclass(frozen=True)
class Message:
    desc: descriptor_pb2.DescriptorProto
    package: str
    parent_names: tuple[str, ...] = ()

    @functools.cached_property
    def type_name(self) -> str:
        """Returns the value returned for MessageLite::GetTypeName()."""
        return '.'.join((self.package, *self.parent_names, self.desc.name))

    @functools.cached_property
    def cpp_name(self) -> str:
        """Returns the fully qualified c++ type name."""
        namespace = self.package.replace('.', '::')
        classname = '_'.join((*self.parent_names, self.desc.name))
        return f'{namespace}::{classname}'

    @functools.cached_property
    def iname(self) -> str:
        """Returns the identifier piece for generated function names."""
        return '_' + self.type_name.replace('.', '_')

    @functools.cached_property
    def fields(self) -> tuple[Field, ...]:
        return tuple(Field(fdesc) for fdesc in self.desc.field)


@dataclasses.dataclass(frozen=True)
class Field:
    desc: descriptor_pb2.FieldDescriptorProto

    @property
    def tag_number(self):
        return self.desc.number

    @property
    def name(self):
        return self.desc.name

    @property
    def type(self) -> int:
        return self.desc.type

    @property
    def is_repeated(self):
        return self.desc.label == 3

    @property
    def typename(self):
        return self.desc.type_name.replace('.', '_')


@dataclasses.dataclass()
class KnownMessages:
    _file_names: set[str] = dataclasses.field(default_factory=set)
    _known: dict[str, Message] = dataclasses.field(default_factory=dict)

    def _AddMessage(self, msg: Message) -> None:
        self._known['.' + msg.type_name] = msg
        for nested_type in msg.desc.nested_type:
            self._AddMessage(
                Message(desc=nested_type,
                        package=msg.package,
                        parent_names=(*msg.parent_names, msg.desc.name)))

    def AddFileDescriptorSet(self,
                             fds: descriptor_pb2.FileDescriptorSet) -> None:
        for f in fds.file:
            if (f.package != 'optimization_guide.proto.registry'):
                self._file_names.add(f.name)
            for m in f.message_type:
                self._AddMessage(Message(desc=m, package=f.package))

    def GetMessageForField(self, field: Field) -> Message:
        return self._known[field.desc.type_name]

    def GetMessages(self, message_types: set[str]) -> list[Message]:
        return [self._known[t] for t in sorted(message_types)]

    def GetAllTransitiveDeps(self, message_types: set[str]) -> list[Message]:
        seen = message_types
        stack = list(message_types)
        while stack:
            msg = self._known[stack.pop()]
            field_types = {
                field.desc.type_name
                for field in msg.fields if field.type == Type.MESSAGE
            }
            stack.extend(field_types - seen)
            seen.update(field_types)
        return self.GetMessages(seen)

    def _GetRegistryMsg(self) -> Message:
        return self._known[
            '.optimization_guide.proto.registry.OnDeviceFeatureProtoRegistry']

    def YieldMessagesWithRole(self, role: str):
        for entry_field in self._GetRegistryMsg().fields:
            logging_data_msg = self._known[entry_field.desc.type_name]
            for field in logging_data_msg.fields:
                if field.name == role:
                    yield field.desc.type_name

    def GetIncludes(self) -> Iterator[str]:
        """Returns the set of includes that cover all known messages types."""
        pattern = re.compile(
            r'.*(components/optimization_guide/proto/.*)\.proto')
        for name in self._file_names:
            if m := pattern.match(name):
                yield m.group(1) + '.pb.h'


def GenerateProtoDescriptors(out: IO[str], messages: KnownMessages):
    """Generate the on_device_model_execution_proto_descriptors.cc content."""

    requests = set(messages.YieldMessagesWithRole('request'))
    responses = set(messages.YieldMessagesWithRole('response'))
    readable_messages = messages.GetAllTransitiveDeps(requests | responses)
    writable_messages = messages.GetAllTransitiveDeps(responses)

    out.write(
        '// DO NOT MODIFY. GENERATED BY gen_on_device_proto_descriptors.py\n')
    out.write('\n')

    out.write(
        '#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"\n'  # pylint: disable=line-too-long
    )
    out.write('\n')

    includes = set(messages.GetIncludes()).union({
        'base/values.h',
        'components/optimization_guide/core/optimization_guide_util.h',
        'components/optimization_guide/core/model_execution/value_converter.h',
    })
    for include in sorted(includes):
        out.write(f'#include "{include}"\n')
    out.write('\n')

    out.write('namespace optimization_guide {\n')
    out.write('\n')
    out.write('namespace {\n')
    _GetProtoValue.GenPrivate(out, readable_messages)
    _SetProtoValueFromString.GenPrivate(out, writable_messages)
    _ConvertValue.GenPrivate(out, writable_messages)
    out.write('}  // namespace\n\n')
    _GetProtoValue.GenPublic(out)
    _GetProtoFromAny.GenPublic(out, readable_messages)
    _BuildMessage.GenPublic(out, writable_messages)
    _SetProtoValueFromString.GenPublic(out)
    _SetProtoFieldString(readable_messages).GenPublic(out)
    _GetProtoMessage(readable_messages).GenPublic(out)
    _GetProtoMutableMessage(readable_messages).GenPublic(out)
    _GetProtoRepeatedSize(readable_messages).GenPublic(out)
    _AddProtoMessage(readable_messages).GenPublic(out)
    _GetProtoMutableRepeatedMessage(readable_messages).GenPublic(out)
    _ConvertValue.GenPublic(out, writable_messages)
    out.write('}  // namespace optimization_guide\n')
    out.write('\n')


class _GetProtoValue:
    """Namespace class for GetProtoValue method builders."""

    @classmethod
    def GenPublic(cls, out: IO[str]):
        out.write("""
          std::optional<proto::Value> GetProtoValue(
              const google::protobuf::MessageLite& msg,
              const proto::ProtoField& proto_field) {
            return GetProtoValue(msg, proto_field, /*index=*/0);
          }
        """)

    @classmethod
    def GenPrivate(cls, out: IO[str], messages: list[Message]):
        out.write("""
          std::optional<proto::Value> GetProtoValue(
              const google::protobuf::MessageLite& msg,
              const proto::ProtoField& proto_field, int32_t index) {
            if (index >= proto_field.proto_descriptors_size()) {
              return std::nullopt;
            }
            int32_t tag_number =
                proto_field.proto_descriptors(index).tag_number();
        """)

        for msg in messages:
            cls._IfMsg(out, msg)
        out.write('return std::nullopt;\n')
        out.write('}\n\n')  # End function

    @classmethod
    def _IfMsg(cls, out: IO[str], msg: Message):
        if msg.type_name == "optimization_guide.proto.Media":
            return
        if all(field.is_repeated for field in msg.fields):
            # Omit the empty case to avoid unused variable warnings.
            return
        out.write(f'if (msg.GetTypeName() == "{msg.type_name}") {{\n')
        out.write(f'const {msg.cpp_name}& casted_msg = ')
        out.write(f'  static_cast<const {msg.cpp_name}&>(msg);\n')
        out.write('switch (tag_number) {\n')
        for field in msg.fields:
            if field.is_repeated:
                continue
            cls._FieldCase(out, field)
        out.write('}\n')  # End switch
        out.write('}\n\n')  # End if statement

    @classmethod
    def _FieldCase(cls, out: IO[str], field: Field):
        out.write(f'case {field.tag_number}: {{\n')
        name = f'casted_msg.{field.name}()'
        if field.type == Type.MESSAGE:
            out.write(f'return GetProtoValue({name}, proto_field, index+1);\n')
        else:
            out.write('proto::Value value;\n')
            if field.type in {Type.DOUBLE, Type.FLOAT}:
                out.write(
                    f'value.set_float_value(static_cast<double>({name}));\n')
            elif field.type in {Type.INT64, Type.UINT64}:
                out.write(
                    f'value.set_int64_value(static_cast<int64_t>({name}));\n')
            elif field.type in {Type.INT32, Type.UINT32, Type.ENUM}:
                out.write(
                    f'value.set_int32_value(static_cast<int32_t>({name}));\n')
            elif field.type in {Type.BOOL}:
                out.write(f'value.set_boolean_value({name});\n')
            elif field.type in {Type.STRING}:
                out.write(f'value.set_string_value({name});\n')
            else:
                raise Error()
            out.write('return value;\n')
        out.write('}\n')  # End case


class _GetProtoFromAny:
    """Namespace class for GetProtoFromAny method builders."""

    @classmethod
    def GenPublic(cls, out: IO[str], messages: list[Message]):
        out.write("""
          std::unique_ptr<google::protobuf::MessageLite> GetProtoFromAny(
              const proto::Any& msg) {
        """)

        for msg in messages:
            cls._IfMsg(out, msg)
        out.write('return nullptr;\n')
        out.write('}\n\n')  # End function

    @classmethod
    def _IfMsg(cls, out: IO[str], msg: Message):
        out.write(f"""if (msg.type_url() ==
                    "type.googleapis.com/{msg.type_name}") {{
            """)
        out.write(
            f'auto casted_msg = ParsedAnyMetadata<{msg.cpp_name}>(msg);\n')
        out.write("""
            std::unique_ptr<google::protobuf::MessageLite> copy(
                casted_msg->New());\n
        """)
        out.write('copy->CheckTypeAndMergeFrom(*casted_msg);\n')
        out.write('return copy;\n')
        out.write('}\n\n')  # End if statement


class _GetProtoMessage:
    """Code generator for _GetProtoMessage."""

    def __init__(self, supported_messages: list[Message]):
        self._supported_messages = supported_messages

    def GenPublic(self, out: IO[str]):
        out.write("""
          const google::protobuf::MessageLite* GetProtoMessage(
              const google::protobuf::MessageLite* msg,
              int32_t tag_number) {
          """)
        for msg in self._supported_messages:
            self._IfMsg(out, msg)
        out.write('return nullptr;\n')
        out.write('}\n\n')  # End function

    def IsSupported(self, field: Field) -> bool:
        return field.type == Type.MESSAGE and not field.is_repeated

    def _IfMsg(self, out: IO[str], msg: Message):
        if not any(self.IsSupported(field) for field in msg.fields):
            return
        out.write(f'if (msg->GetTypeName() == "{msg.type_name}") {{\n')
        out.write(
            f'auto* typed_msg = static_cast<const {msg.cpp_name}*>(msg);\n')
        out.write('switch (tag_number) {\n')
        for field in msg.fields:
            if self.IsSupported(field):
                self.FieldCase(out, msg, field)
        out.write('}\n')  # End switch
        out.write('}\n\n')  # End if statement

    def FieldCase(self, out: IO[str], msg: Message, field: Field):
        out.write(f'case {field.tag_number}: {{\n')
        out.write(f'return &typed_msg->{field.name}();\n')
        out.write('}\n')  # End case


class _GetProtoMutableMessage:
    """Code generator for _GetProtoMutableMessage."""

    def __init__(self, supported_messages: list[Message]):
        self._supported_messages = supported_messages

    def GenPublic(self, out: IO[str]):
        out.write("""
          google::protobuf::MessageLite* GetProtoMutableMessage(
              google::protobuf::MessageLite* msg,
              int32_t tag_number) {
          """)
        for msg in self._supported_messages:
            self._IfMsg(out, msg)
        out.write('return nullptr;\n')
        out.write('}\n\n')  # End function

    def IsSupported(self, field: Field) -> bool:
        return field.type == Type.MESSAGE and not field.is_repeated

    def _IfMsg(self, out: IO[str], msg: Message):
        if not any(self.IsSupported(field) for field in msg.fields):
            return
        out.write(f'if (msg->GetTypeName() == "{msg.type_name}") {{\n')
        out.write(f'auto* typed_msg = static_cast<{msg.cpp_name}*>(msg);\n')
        out.write('switch (tag_number) {\n')
        for field in msg.fields:
            if self.IsSupported(field):
                self.FieldCase(out, msg, field)
        out.write('}\n')  # End switch
        out.write('}\n\n')  # End if statement

    def FieldCase(self, out: IO[str], msg: Message, field: Field):
        out.write(f'case {field.tag_number}: {{\n')
        out.write(f'return typed_msg->mutable_{field.name}();\n')
        out.write('}\n')  # End case


class _AddProtoMessage:
    """Code generator for _AddProtoMessage."""

    def __init__(self, supported_messages: list[Message]):
        self._supported_messages = supported_messages

    def GenPublic(self, out: IO[str]):
        out.write("""
          int AddProtoMessage(
              google::protobuf::MessageLite* msg,
              int32_t tag_number) {
          """)
        for msg in self._supported_messages:
            self._IfMsg(out, msg)
        out.write('return 0;\n')
        out.write('}\n\n')  # End function

    def IsSupported(self, field: Field) -> bool:
        return field.type == Type.MESSAGE and field.is_repeated

    def _IfMsg(self, out: IO[str], msg: Message):
        if not any(self.IsSupported(field) for field in msg.fields):
            return
        out.write(f'if (msg->GetTypeName() == "{msg.type_name}") {{\n')
        out.write(f'auto* typed_msg = static_cast<{msg.cpp_name}*>(msg);\n')
        out.write('switch (tag_number) {\n')
        for field in msg.fields:
            if self.IsSupported(field):
                self.FieldCase(out, msg, field)
        out.write('}\n')  # End switch
        out.write('}\n\n')  # End if statement

    def FieldCase(self, out, msg: Message, field: Field):
        out.write(f'case {field.tag_number}: {{\n')
        out.write(f'typed_msg->add_{field.name}();\n')
        out.write(f'return typed_msg->{field.name}_size();\n')
        out.write('}\n')  # End case


class _GetProtoRepeatedSize:
    """Code generator for _AddProtoMessage."""

    def __init__(self, supported_messages: list[Message]):
        self._supported_messages = supported_messages

    def GenPublic(self, out: IO[str]):
        out.write("""
          int GetProtoRepeatedSize(
              const google::protobuf::MessageLite* msg,
              int32_t tag_number) {
          """)
        for msg in self._supported_messages:
            self._IfMsg(out, msg)
        out.write('return 0;\n')
        out.write('}\n\n')  # End function

    def IsSupported(self, field: Field) -> bool:
        return field.type == Type.MESSAGE and field.is_repeated

    def _IfMsg(self, out: IO[str], msg: Message):
        if not any(self.IsSupported(field) for field in msg.fields):
            return
        out.write(f'if (msg->GetTypeName() == "{msg.type_name}") {{\n')
        out.write(
            f'auto* typed_msg = static_cast<const {msg.cpp_name}*>(msg);\n')
        out.write('switch (tag_number) {\n')
        for field in msg.fields:
            if self.IsSupported(field):
                self.FieldCase(out, msg, field)
        out.write('}\n')  # End switch
        out.write('}\n\n')  # End if statement

    def FieldCase(self, out: IO[str], msg: Message, field: Field):
        out.write(f'case {field.tag_number}: {{\n')
        out.write(f'return typed_msg->{field.name}_size();\n')
        out.write('}\n')  # End case


class _GetProtoMutableRepeatedMessage:
    """Namespace class for GetProtoMutableRepeatedMessage method builder."""

    def __init__(self, supported_messages: list[Message]):
        self._supported_messages = supported_messages

    def GenPublic(self, out: IO[str]):
        out.write("""
          google::protobuf::MessageLite* GetProtoMutableRepeatedMessage(
              google::protobuf::MessageLite* parent,
              int32_t tag_number,
              int offset) {\n
        """)
        for msg in self._supported_messages:
            self._IfMsg(out, msg)
        out.write('return nullptr;\n')
        out.write('}\n\n')  # End function

    def IsSupported(self, field: Field) -> bool:
        return field.type == Type.MESSAGE and field.is_repeated

    def _IfMsg(self, out: IO[str], msg: Message):
        if not any(self.IsSupported(field) for field in msg.fields):
            return
        out.write(f'if (parent->GetTypeName() == "{msg.type_name}") {{\n')
        out.write(f'auto* typed_msg = static_cast<{msg.cpp_name}*>(parent);\n')
        out.write('switch (tag_number) {\n')
        for field in msg.fields:
            if self.IsSupported(field):
                self.FieldCase(out, msg, field)
        out.write('}\n')  # End switch
        out.write('}\n\n')  # End if statement

    def FieldCase(self, out: IO[str], msg: Message, field: Field):
        out.write(f'case {field.tag_number}: {{\n')
        out.write(f'if (offset >= typed_msg->{field.name}_size()) {{'
                  f'return nullptr; }};\n')
        out.write(f'return typed_msg->mutable_{field.name}(offset);\n')
        out.write('}\n')  # End case


class _SetProtoFieldString:
    """Code generator for SetProtoField with a string argument."""

    def __init__(self, supported_messages: list[Message]):
        self._supported_messages = supported_messages

    def GenPublic(self, out: IO[str]):
        out.write("""
          ProtoStatus SetProtoField(
              google::protobuf::MessageLite* msg,
              int32_t tag_number,
              const std::string& value) {
          """)
        for msg in self._supported_messages:
            self._IfMsg(out, msg)
        out.write('return ProtoStatus::kError;\n')
        out.write('}\n\n')  # End function

    def IsSupported(self, field: Field) -> bool:
        return field.type == Type.STRING and not field.is_repeated

    def _IfMsg(self, out, msg: Message):
        if not any(self.IsSupported(field) for field in msg.fields):
            return
        out.write(f'if (msg->GetTypeName() == "{msg.type_name}") {{\n')
        out.write(f'auto* typed_msg = static_cast<{msg.cpp_name}*>(msg);\n')
        out.write('switch (tag_number) {\n')
        for field in msg.fields:
            if self.IsSupported(field):
                self.FieldCase(out, msg, field)
        out.write('}\n')  # End switch
        out.write('}\n\n')  # End if statement

    def FieldCase(self, out, msg: Message, field: Field):
        out.write(f'case {field.tag_number}: {{\n')
        out.write(f'typed_msg->set_{field.name}(value);\n')
        out.write(f'return ProtoStatus::kOk;\n')
        out.write('}\n')  # End case


class _BuildMessage:
    """Namespace class for BuildMessage method builders."""

    @classmethod
    def GenPublic(cls, out: IO[str], messages: list[Message]):
        out.write('std::unique_ptr<google::protobuf::MessageLite> ')
        out.write('BuildMessage(\n')
        out.write('    const std::string& proto_name) {\n')
        for msg in messages:
            cls._IfMsg(out, msg)
        out.write('  return nullptr;\n')
        out.write('}\n')

    @classmethod
    def _IfMsg(cls, out: IO[str], msg: Message):
        out.write(f'if (proto_name == "{msg.type_name}") {{\n')
        out.write(f'  return std::make_unique<{msg.cpp_name}>();\n')
        out.write('}\n')


class _SetProtoValueFromString:
    """Namespace class for SetProtoValueFromString method builders."""

    @classmethod
    def GenPublic(cls, out: IO[str]):
        out.write("""
      ProtoStatus SetProtoValueFromString(
          google::protobuf::MessageLite* msg,
          const proto::ProtoField& proto_field,
          const std::string& value) {
        return SetProtoValueFromString(msg, proto_field, value, /*index=*/0);
      }
    """)

    @classmethod
    def GenPrivate(cls, out: IO[str], messages: list[Message]):
        out.write("""
      ProtoStatus SetProtoValueFromString(
          google::protobuf::MessageLite* msg,
          const proto::ProtoField& proto_field,
          const std::string& value,
          int32_t index) {
        if (index >= proto_field.proto_descriptors_size()) {
          return ProtoStatus::kError;
        }
    """)
        for msg in messages:
            cls._IfMsg(out, msg)
        out.write("""
        return ProtoStatus::kError;
      }
    """)

    @classmethod
    def _IsSupported(cls, field: Field):
        # TODO(https://crbug.com/383761415): Implement the enum case.
        return (not field.is_repeated
                and field.type not in (Type.MESSAGE, Type.ENUM))

    @classmethod
    def _IfMsg(cls, out: IO[str], msg: Message):
        if not any(cls._IsSupported(field) for field in msg.fields):
            return

        out.write(f'if (msg->GetTypeName() == "{msg.type_name}") {{\n')
        out.write(f'  auto* typed_msg = static_cast<{msg.cpp_name}*>(msg);\n')
        out.write(
            '  switch (proto_field.proto_descriptors(index).tag_number()) {\n')
        for field in msg.fields:
            if cls._IsSupported(field):
                cls._FieldCase(out, msg, field)
        out.write('  }\n')
        out.write('  return ProtoStatus::kError;\n')
        out.write('}\n')  # End if statement

    @classmethod
    def _FieldCase(cls, out: IO[str], msg: Message, field: Field):
        out.write(f'    case {field.tag_number}: {{\n')
        if field.type == Type.STRING:
            out.write(f'      typed_msg->set_{field.name}(value);\n')
        else:
            out.write('      using FieldType = '
                      f'decltype(typed_msg->{field.name}());\n')
            out.write(
                '      base::expected<FieldType, ProtoStatus> result =\n'
                '          ValueConverter<FieldType>::TryConvertFromString('
                'value);\n')
            out.write('      if (!result.has_value()) {\n')

            out.write('        return ProtoStatus::kError;\n')
            out.write('      }\n')
            out.write(f'      typed_msg->set_{field.name}(result.value());\n')
        out.write('      return ProtoStatus::kOk;\n')
        out.write('    }\n')


class _ConvertValue:
    """Namespace class for base::Value->Message method builders."""

    @classmethod
    def GenPublic(cls, out: IO[str], messages: list[Message]):
        out.write(f"""
          std::optional<proto::Any> ConvertToAnyWrappedProto(
              const base::Value& object, const std::string& type_name) {{
            proto::Any any;
            any.set_type_url("type.googleapis.com/" + type_name);
        """)
        for msg in messages:
            out.write(f"""
            if (type_name == "{msg.type_name}") {{
              {msg.cpp_name} msg;
              if (Convert{msg.iname}(object, msg)) {{
                msg.SerializeToString(any.mutable_value());
                return any;
              }}
            }}
          """)

        out.write(f"""
            return std::nullopt;
          }}
        """)

    @classmethod
    def GenPrivate(cls, out: IO[str], messages: list[Message]):
        for msg in messages:
            out.write(f"""
            bool Convert{msg.iname}(
                const base::Value& object, {msg.cpp_name}& proto);
          """)
        for msg in messages:
            cls._DefineConvert(out, msg)

    @classmethod
    def _DefineConvert(cls, out: IO[str], msg: Message):
        out.write(f"""
          bool Convert{msg.iname}(
              const base::Value& object, {msg.cpp_name}& proto) {{
            const base::Value::Dict* asdict = object.GetIfDict();
            if (!asdict) {{
              return false;
            }}
        """)
        for field in msg.fields:
            if field.type == Type.GROUP:
                continue
            if field.type == Type.ENUM:
                continue
            out.write('if (const base::Value* field_value =\n')
            out.write(f'     asdict->Find("{field.desc.json_name}")) {{')
            cls._FieldCase(out, msg, field)
            out.write(f'}}')
        out.write(f"""
            return true;
          }}
        """)

    @classmethod
    def _FieldCase(cls, out: IO[str], msg: Message, field: Field):
        if field.is_repeated:
            out.write(f"""
              const auto* lst = field_value->GetIfList();
              if (!lst) {{
                return false;
              }}
              for (const base::Value& entry_value : *lst) {{
            """)
            if field.type == Type.MESSAGE:
                out.write(f"""
                  if (!Convert{field.typename}(
                      entry_value, *proto.add_{field.name}())) {{
                    return false;
                  }}
                """)
            else:
                vtype = BASE_VALUE_TYPES[field.type]
                out.write(f"""
                  const {vtype.cpptype} v = entry_value.GetIf{vtype.getIfFn}();
                  if (!v) {{
                    return false;
                  }}
                  proto.add_{field.name}(*v);
                """)
            out.write("}")  # end for loop
        else:
            if field.type == Type.MESSAGE:
                out.write(f"""
                  if (!Convert{field.typename}(
                      *field_value, *proto.mutable_{field.name}())) {{
                    return false;
                  }}
                """)
                return
            else:
                vtype = BASE_VALUE_TYPES[field.type]
                out.write(f"""
                  const {vtype.cpptype} v = field_value->GetIf{vtype.getIfFn}();
                  if (!v) {{
                    return false;
                  }}
                  proto.set_{field.name}(*v);
                """)


def main(argv):
    parser = optparse.OptionParser()
    parser.add_option('--input_file', action='append', default=[])
    parser.add_option('--output_cc')
    options, _ = parser.parse_args(argv)

    input_files = list(options.input_file)

    # Write to standard output or file specified by --output_cc.
    out_cc = getattr(sys.stdout, 'buffer', sys.stdout)
    if options.output_cc:
        out_cc = open(options.output_cc, 'wb')

    messages = KnownMessages()
    for input_file in input_files:
        fds = descriptor_pb2.FileDescriptorSet()
        with open(input_file, 'rb') as fp:
            fds.ParseFromString(fp.read())
            messages.AddFileDescriptorSet(fds)

    out_cc_str = StringIO()
    GenerateProtoDescriptors(out_cc_str, messages)
    out_cc.write(out_cc_str.getvalue().encode('utf-8'))

    if options.output_cc:
        out_cc.close()

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

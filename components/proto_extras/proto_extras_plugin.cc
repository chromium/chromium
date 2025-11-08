// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/protobuf/src/google/protobuf/compiler/code_generator.h"
#include "third_party/protobuf/src/google/protobuf/compiler/cpp/helpers.h"
#include "third_party/protobuf/src/google/protobuf/compiler/cpp/names.h"
#include "third_party/protobuf/src/google/protobuf/compiler/importer.h"
#include "third_party/protobuf/src/google/protobuf/compiler/plugin.h"
#include "third_party/protobuf/src/google/protobuf/descriptor.h"
#include "third_party/protobuf/src/google/protobuf/io/printer.h"

namespace {

using google::protobuf::Descriptor;
using google::protobuf::Edition;
using google::protobuf::FieldDescriptor;
using google::protobuf::FileDescriptor;
using google::protobuf::OneofDescriptor;
using google::protobuf::compiler::GeneratorContext;
using google::protobuf::compiler::cpp::ClassName;
using google::protobuf::compiler::cpp::FieldName;
using google::protobuf::compiler::cpp::Namespace;
using google::protobuf::compiler::cpp::NamespaceOpener;
using google::protobuf::compiler::cpp::QualifiedClassName;
using google::protobuf::compiler::cpp::UnderscoresToCamelCase;
using google::protobuf::io::Printer;
using google::protobuf::io::ZeroCopyOutputStream;

struct ProtoExtrasGeneratorOptions {
  bool generate_to_value_serialization = false;
  bool generate_stream_operator = false;
  bool generate_equality = false;
  bool protobuf_full_support = false;
};

bool GetAllClassNames(const Descriptor& message,
                      base::flat_set<std::string>& class_names) {
  class_names.insert(ClassName(&message));
  for (int i = 0; i < message.nested_type_count(); i++) {
    if (!GetAllClassNames(*message.nested_type(i), class_names)) {
      return false;
    }
  }
  return true;
}

void FieldToMapKeyFunction(const FieldDescriptor* field, Printer* printer) {
  using enum FieldDescriptor::Type;
  // From:
  // - https://protobuf.dev/programming-guides/proto3/#maps
  // - https://protobuf.dev/programming-guides/proto2/#maps
  // > `key_type` can be any integral or string type (so, any scalar type except
  // > for floating point types and bytes). Note that neither enum nor proto
  // > messages are valid for `key_type`. The `value_type` can be any type
  // except > another map.
  auto conversion_function = [&]() -> std::string {
    switch (field->type()) {
      case TYPE_STRING:
        return "static_cast<std::string>";
      case TYPE_INT32:
      case TYPE_INT64:
      case TYPE_UINT64:
      case TYPE_UINT32:
      case TYPE_FIXED64:
      case TYPE_FIXED32:
      case TYPE_SFIXED64:
      case TYPE_SFIXED32:
      case TYPE_SINT64:
      case TYPE_SINT32:
        return "base::NumberToString";
      case TYPE_BOOL:
      case TYPE_BYTES:
      case TYPE_ENUM:
      case TYPE_DOUBLE:
      case TYPE_FLOAT:
      case TYPE_MESSAGE:
      case TYPE_GROUP:
        NOTREACHED() << "Invalid protobuf map key type.";
    }
    NOTREACHED();
  };
  printer->Print(conversion_function());
}

void FieldToValueFunction(const FieldDescriptor* field, Printer* printer) {
  using enum FieldDescriptor::CppType;
  using enum FieldDescriptor::Type;
  using enum FieldDescriptor::CppStringType;
  auto conversion_function = [&]() -> std::string {
    switch (field->cpp_type()) {
      case CPPTYPE_DOUBLE:
      case CPPTYPE_FLOAT:
        return "static_cast<double>";
      case CPPTYPE_INT32:
        // No function needed.
        return "";
      case CPPTYPE_UINT32:
      case CPPTYPE_INT64:
      case CPPTYPE_UINT64:
        return "::proto_extras::ToNumericTypeForValue";
      case CPPTYPE_BOOL:
        return "static_cast<bool>";
      case CPPTYPE_STRING: {
        bool output_bytes = field->type() == TYPE_BYTES;
        switch (field->cpp_string_type()) {
          case kView:
          case kString:
            if (output_bytes) {
              return "base::Base64Encode";
            } else {
              // No function needed.
              return "";
            }
          case kCord:
            CHECK(output_bytes) << "kCord is only supported for bytes fields.";
            // If cord support is enabled for strings, this should probably
            // return "std::string".
            return "::proto_extras::Base64EncodeCord";
        }
      }
      case CPPTYPE_ENUM:
        return base::StrCat({QualifiedClassName(field->enum_type()), "_Name"});
      case CPPTYPE_MESSAGE:
        // The ToValue function for the message is in the namespace of the
        // nested message itself.
        return base::StrCat({Namespace(field->message_type()), "::ToValue"});
    }
    NOTREACHED();
  };
  printer->Print(conversion_function());
}

void CreateToValueSerializationDefinitions(
    const Descriptor& message,
    Printer* printer,
    const ProtoExtrasGeneratorOptions& options) {
  printer->Emit(
      {{"message_type", ClassName(&message)},
       {"to_value_fields",
        [&]() {
          for (int j = 0; j < message.field_count(); j++) {
            const FieldDescriptor* field = message.field(j);
            std::string field_name = FieldName(message.field(j));

            auto field_to_value = [&]() {
              FieldToValueFunction(field, printer);
            };
            if (field->is_map()) {
              const FieldDescriptor* map_value =
                  field->message_type()->map_value();
              const FieldDescriptor* map_key = field->message_type()->map_key();
              printer->Emit(
                  {{"field_name", field_name},
                   {"map_key_to_value",
                    [&] { FieldToMapKeyFunction(map_key, printer); }},
                   {"map_value_to_value",
                    [&] { FieldToValueFunction(map_value, printer); }}},
                  R"(
  if (!message.$field_name$().empty()) {
    base::DictValue map_dict;
    for (const auto& [key, value] : message.$field_name$()) {
      map_dict.Set($map_key_to_value$(key), $map_value_to_value$(value));
    }
    dict.Set("$field_name$", std::move(map_dict));
  }
)");
            } else if (field->is_repeated()) {
              printer->Emit({{"field_name", field_name},
                             {"field_to_value", field_to_value}},
                            R"(
  if (!message.$field_name$().empty()) {
    base::ListValue list;
    for (const auto& value : message.$field_name$()) {
      list.Append($field_to_value$(value));
    }
    dict.Set("$field_name$", std::move(list));
  }
)");
            } else if (field->has_presence()) {
              printer->Emit({{"field_name", field_name},
                             {"field_to_value", field_to_value}},
                            R"(
  if (message.has_$field_name$()) {
    dict.Set("$field_name$", $field_to_value$(message.$field_name$()));
  }
)");
            } else if (field->type() == FieldDescriptor::Type::TYPE_STRING ||
                       field->type() == FieldDescriptor::Type::TYPE_BYTES) {
              printer->Emit({{"field_name", field_name},
                             {"field_to_value", field_to_value}},
                            R"(
  if (!message.$field_name$().empty()) {
    dict.Set("$field_name$", $field_to_value$(message.$field_name$()));
  }
)");
            } else {
              printer->Emit({{"field_name", field_name},
                             {"field_to_value", field_to_value}},
                            R"(
  dict.Set("$field_name$", $field_to_value$(message.$field_name$()));
)");
            }
          }
        }}},
      R"(
base::Value ToValue(const $message_type$& message) {
  base::DictValue dict;
  ::proto_extras::SerializeUnknownFields(message, dict);
  $to_value_fields$
  return base::Value(std::move(dict));
}
void MaybeToValue(const std::optional<$message_type$>& opt_message,
                    std::string_view name,
                    base::DictValue& output_dictionary) {
  if (!opt_message.has_value()) {
    return;
  }
  output_dictionary.Set(name, ToValue(*opt_message));
}
)");
}

void CreateOstreamDefinition(const Descriptor& message,
                             Printer* printer,
                             const ProtoExtrasGeneratorOptions& options) {
  std::string message_type = ClassName(&message);
  printer->Emit({{"message_type", message_type}},
                R"(
std::ostream& operator<<(std::ostream& out, const $message_type$& message) {
  // This relies on ToValue() from *.to_value.h.
  return out << ToValue(message).DebugString();
}
)");
}

void CreateEqualityOperatorDefinition(
    const Descriptor& message,
    Printer* printer,
    const ProtoExtrasGeneratorOptions& options) {
  std::string message_type = ClassName(&message);
  printer->Emit(
      {{"message_type", message_type},
       {"compare_fields",
        [&]() {
          // If protobuf_full_support is enabled, use MessageDifferencerEquals
          // to compare the messages as the messages should be full Message
          // types.
          if (options.protobuf_full_support) {
            printer->Print(
                "if (!::proto_extras::MessageDifferencerEquals(lhs, rhs)) "
                "return false;\n");
            return;
          }
          printer->Print(
              "if (lhs.unknown_fields() != rhs.unknown_fields()) return "
              "false;\n");

          // Compare oneof fields using a switch statement.
          // Skip synthetic oneofs.
          // (see implementing_proto3_presence.md#to-iterate-over-all-oneofs)
          for (int i = 0; i < message.real_oneof_decl_count(); ++i) {
            const OneofDescriptor* oneof = message.oneof_decl(i);
            printer->Emit(
                {{"oneof_name", oneof->name()},
                 {"message_type", message_type},
                 {"captital_oneof_name", base::ToUpperASCII(oneof->name())},
                 {"body",
                  [&]() {
                    for (int j = 0; j < oneof->field_count(); ++j) {
                      const FieldDescriptor* field = oneof->field(j);
                      std::string field_name = FieldName(field);
                      std::string case_name = UnderscoresToCamelCase(
                          field->lowercase_name(), /*cap_next_letter=*/true);

                      printer->Emit(
                          {
                              {"message_type", message_type},
                              {"case_name", case_name},
                              {"field_name", field_name},
                          },
                          R"(
          case $message_type$::k$case_name$:
            if (lhs.$field_name$() != rhs.$field_name$()) return false;
            break;
      )");
                    }
                  }}},
                R"(
  if (lhs.$oneof_name$_case() != rhs.$oneof_name$_case()) return false;
  switch (lhs.$oneof_name$_case()) {
    $body$
    case $message_type$::$captital_oneof_name$_NOT_SET:
      break;
  }
)");
          }

          // Compare non-oneof fields.
          for (int j = 0; j < message.field_count(); j++) {
            const FieldDescriptor* field = message.field(j);
            // Skip fields that are part of a oneof, as they are handled above.
            if (field->real_containing_oneof()) {
              continue;
            }

            std::string field_name = FieldName(field);
            if (field->is_map()) {
              printer->Emit({{"field_name", field_name}},
                            R"(
if (lhs.$field_name$().size() != rhs.$field_name$().size()) return false;
for (const auto& [key, value] : lhs.$field_name$()) {
  auto it = rhs.$field_name$().find(key);
  if (it == rhs.$field_name$().end()) return false;
  if (value != it->second) return false;
}
)");
            } else if (field->is_repeated()) {
              printer->Emit({{"field_name", field_name}},
                            R"(
  if (lhs.$field_name$().size() != rhs.$field_name$().size()) return false;
  for (int i = 0; i < lhs.$field_name$().size(); ++i) {
    if (lhs.$field_name$()[i] != rhs.$field_name$()[i]) return false;
  }
)");
            } else if (field->has_presence()) {
              printer->Emit({{"field_name", field_name}},
                            R"(
  if (lhs.has_$field_name$() != rhs.has_$field_name$()) return false;
  if (lhs.has_$field_name$() && lhs.$field_name$() != rhs.$field_name$()) return false;
)");
            } else {
              printer->Emit({{"field_name", field_name}},
                            R"(
  if (lhs.$field_name$() != rhs.$field_name$()) return false;
)");
            }
          }
        }}},
      R"(
bool operator==(const $message_type$& lhs, const $message_type$& rhs) {
  if (&lhs == &rhs) return true;
  $compare_fields$
  return true;
}

bool operator!=(const $message_type$& lhs, const $message_type$& rhs) {
  return !(lhs == rhs);
}
)");
}

// Returns if the descriptor is for a synthetic 'map entry' message type,
// which is internally created by the protobuf library to support map fields.
// Map fields are instead handled explicitly in the generation via the
// `is_map()` case.
bool IsSyntheticMapEntry(const Descriptor& message) {
  return message.map_key() != nullptr;
}

class ProtoExtrasGenerator : public google::protobuf::compiler::CodeGenerator {
 public:
  ProtoExtrasGenerator() = default;
  ~ProtoExtrasGenerator() override = default;

  uint64_t GetSupportedFeatures() const override {
    return FEATURE_PROTO3_OPTIONAL | FEATURE_SUPPORTS_EDITIONS;
  }

  Edition GetMinimumEdition() const override { return Edition::EDITION_PROTO2; }

  Edition GetMaximumEdition() const override { return Edition::EDITION_2024; }

  bool Generate(const FileDescriptor* file,
                const std::string& command_line_options,
                GeneratorContext* context,
                std::string* error) const override {
    CHECK(file);

    ProtoExtrasGeneratorOptions generator_options{
        .generate_to_value_serialization = base::Contains(
            command_line_options, "generate_to_value_serialization"),
        .generate_stream_operator =
            base::Contains(command_line_options, "generate_stream_operator"),
        .generate_equality =
            base::Contains(command_line_options, "generate_equality"),
        .protobuf_full_support =
            base::Contains(command_line_options, "protobuf_full_support"),
    };
    // The current design of this library assumes that only one of the
    // serialization options is enabled.
    CHECK(generator_options.generate_to_value_serialization ^
          generator_options.generate_equality ^
          generator_options.generate_stream_operator);

    base::FilePath proto_file_path = base::FilePath::FromASCII(file->name());
    base::FilePath::StringType file_suffix;
    if (generator_options.generate_to_value_serialization) {
      file_suffix = FILE_PATH_LITERAL(".to_value");
    } else if (generator_options.generate_stream_operator) {
      file_suffix = FILE_PATH_LITERAL(".ostream");
    } else {
      CHECK(generator_options.generate_equality);
      file_suffix = FILE_PATH_LITERAL(".equal");
    }

    base::FilePath h_file_path =
        proto_file_path.ReplaceExtension(file_suffix + FILE_PATH_LITERAL(".h"));
    base::FilePath cc_file_path = proto_file_path.ReplaceExtension(
        file_suffix + FILE_PATH_LITERAL(".cc"));

    const std::unique_ptr<ZeroCopyOutputStream> h_stream(
        context->Open(h_file_path.AsUTF8Unsafe()));
    const std::unique_ptr<ZeroCopyOutputStream> cc_stream(
        context->Open(cc_file_path.AsUTF8Unsafe()));

    Printer h_printer(h_stream.get(), Printer::Options{'$', nullptr});
    Printer cc_printer(cc_stream.get(), Printer::Options{'$', nullptr});

    std::string include_guard =
        base::ToUpperASCII(h_file_path.AsUTF8Unsafe()) + "_";
    CHECK(base::ReplaceChars(include_guard, ".-/\\", "_", &include_guard));

    auto forward_declarations = [&]() {
      if (generator_options.generate_to_value_serialization) {
        NamespaceOpener ns("base", &h_printer);
        h_printer.Print("class DictValue;\nclass Value;\n");
      }
      NamespaceOpener ns(Namespace(file), &h_printer);
      base::flat_set<std::string> forward_declarations;
      for (int i = 0; i < file->message_type_count(); i++) {
        GetAllClassNames(*file->message_type(i), forward_declarations);
      }
      for (const auto& forward_declaration : forward_declarations) {
        h_printer.Print("class $class$;\n", "class", forward_declaration);
      }
    };

    h_printer.Emit(
        {
            {"include_guard", include_guard},
            {"proto_file_path", proto_file_path.AsUTF8Unsafe()},
            {"includes",
             [&] {
               if (generator_options.generate_stream_operator) {
                 h_printer.Print("#include <iosfwd>\n\n");
               }
               if (generator_options.generate_to_value_serialization) {
                 h_printer.Print(
                     "#include <optional>\n#include <string_view>\n\n");
               }
             }},
            {"forward_declarations", forward_declarations},
            {"function_declarations",
             [&] {
               NamespaceOpener ns(Namespace(file), &h_printer);
               for (int i = 0; i < file->message_type_count(); i++) {
                 PrintFunctionDeclarations(*file->message_type(i), &h_printer,
                                           error, generator_options);
               }
             }},
        },
        R"(// Generated by the proto_extras plugin. DO NOT EDIT!
// source: $proto_file_path$

#ifndef $include_guard$
#define $include_guard$

$includes$

$forward_declarations$

$function_declarations$

#endif  // $include_guard$
)");

    // Determine the #includes for the implementation file.
    base::flat_set<std::string> impl_system_includes;
    bool needs_pb_h = generator_options.generate_to_value_serialization ||
                      generator_options.generate_equality;
    base::flat_set<std::string> impl_user_includes;
    if (needs_pb_h) {
      impl_user_includes.insert(
          proto_file_path.ReplaceExtension(FILE_PATH_LITERAL("pb.h"))
              .AsUTF8Unsafe());
    }
    if (generator_options.generate_stream_operator) {
      impl_system_includes.insert("<ostream>");
      impl_user_includes.insert(
          proto_file_path.ReplaceExtension(FILE_PATH_LITERAL("to_value.h"))
              .AsUTF8Unsafe());
      impl_user_includes.insert("base/values.h");
    }
    if (generator_options.generate_to_value_serialization) {
      impl_user_includes.insert("base/base64.h");
      impl_user_includes.insert("base/strings/string_number_conversions.h");
      impl_user_includes.insert("base/values.h");
      impl_user_includes.insert("components/proto_extras/proto_extras_lib.h");
    }
    for (int i = 0; i < file->dependency_count(); i++) {
      base::FilePath dependency_proto_file_path =
          base::FilePath::FromASCII(file->dependency(i)->name());
      if (needs_pb_h) {
        impl_user_includes.insert(
            dependency_proto_file_path
                .ReplaceExtension(FILE_PATH_LITERAL("pb.h"))
                .AsUTF8Unsafe());
      }
      if (generator_options.generate_to_value_serialization) {
        impl_user_includes.insert(
            dependency_proto_file_path
                .ReplaceExtension(FILE_PATH_LITERAL("to_value.h"))
                .AsUTF8Unsafe());
      } else if (generator_options.generate_equality) {
        impl_user_includes.insert(
            dependency_proto_file_path
                .ReplaceExtension(FILE_PATH_LITERAL("equal.h"))
                .AsUTF8Unsafe());
      }
    }
    if (generator_options.protobuf_full_support) {
      impl_user_includes.insert(
          "components/proto_extras/protobuf_full_support.h");
    }
    cc_printer.Emit(
        {
            {"proto_file_path", proto_file_path.AsUTF8Unsafe()},
            {"header_file_path", h_file_path.AsUTF8Unsafe()},
            {"system_includes",
             [&] {
               for (const auto& include : impl_system_includes) {
                 cc_printer.Print("#include $f$\n", "f", include);
               }
             }},
            {"user_includes",
             [&] {
               for (const auto& include : impl_user_includes) {
                 cc_printer.Print("#include \"$f$\"\n", "f", include);
               }
             }},
            {"function_definitions",
             [&] {
               NamespaceOpener ns(Namespace(file), &cc_printer);
               for (int i = 0; i < file->message_type_count(); i++) {
                 PrintFunctionDefinitions(*file->message_type(i), &cc_printer,
                                          error, generator_options);
               }
             }},
        },
        R"(// Generated by the proto_extras plugin. DO NOT EDIT!
// source: $proto_file_path$

#include "$header_file_path$"

$system_includes$

$user_includes$

$function_definitions$
)");
    return true;
  }

  bool PrintFunctionDeclaration(
      const Descriptor& message,
      Printer* printer,
      std::string* error,
      const ProtoExtrasGeneratorOptions& options) const {
    if (IsSyntheticMapEntry(message)) {
      return true;
    }
    std::string message_type = ClassName(&message);
    if (options.generate_to_value_serialization) {
      printer->Print("base::Value ToValue(const $m$& message);", "m",
                     message_type);
      printer->Print(
          R"(
void MaybeToValue(const std::optional<$m$>& opt_message,
                  std::string_view output_dictionary_field_name,
                  base::DictValue& output_dictionary);
)",
          "m", message_type);
    }
    if (options.generate_stream_operator) {
      printer->Print(
          "std::ostream& operator<<(std::ostream& out, const "
          "$m$& message);\n",
          "m", message_type);
    }
    if (options.generate_equality) {
      printer->Print("bool operator==(const $m$& lhs, const $m$& rhs);\n", "m",
                     message_type);
    }
    return true;
  }

  bool PrintFunctionDefinition(
      const Descriptor& message,
      Printer* printer,
      std::string* error,
      const ProtoExtrasGeneratorOptions& options) const {
    if (IsSyntheticMapEntry(message)) {
      return true;
    }
    if (options.generate_to_value_serialization) {
      CreateToValueSerializationDefinitions(message, printer, options);
    }
    if (options.generate_stream_operator) {
      CreateOstreamDefinition(message, printer, options);
    }
    if (options.generate_equality) {
      CreateEqualityOperatorDefinition(message, printer, options);
    }
    return true;
  }

  bool PrintFunctionDeclarations(
      const Descriptor& message,
      Printer* printer,
      std::string* error,
      const ProtoExtrasGeneratorOptions& options) const {
    if (!PrintFunctionDeclaration(message, printer, error, options)) {
      return false;
    }
    for (int i = 0; i < message.nested_type_count(); i++) {
      if (!PrintFunctionDeclarations(*message.nested_type(i), printer, error,
                                     options)) {
        return false;
      }
    }
    return true;
  }

  bool PrintFunctionDefinitions(
      const Descriptor& message,
      Printer* printer,
      std::string* error,
      const ProtoExtrasGeneratorOptions& options) const {
    if (!PrintFunctionDefinition(message, printer, error, options)) {
      return false;
    }
    for (int i = 0; i < message.nested_type_count(); i++) {
      if (!PrintFunctionDefinitions(*message.nested_type(i), printer, error,
                                    options)) {
        return false;
      }
    }
    return true;
  }
};
}  // namespace

int main(int argc, char** argv) {
  ProtoExtrasGenerator generator;
  return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}

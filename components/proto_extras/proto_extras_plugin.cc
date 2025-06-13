// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/protobuf/src/google/protobuf/compiler/code_generator.h"
#include "third_party/protobuf/src/google/protobuf/compiler/cpp/helpers.h"
#include "third_party/protobuf/src/google/protobuf/compiler/cpp/names.h"
#include "third_party/protobuf/src/google/protobuf/compiler/importer.h"
#include "third_party/protobuf/src/google/protobuf/compiler/plugin.h"
#include "third_party/protobuf/src/google/protobuf/descriptor.h"
#include "third_party/protobuf/src/google/protobuf/io/printer.h"

namespace {

using google::protobuf::Descriptor;
using google::protobuf::FieldDescriptor;
using google::protobuf::FileDescriptor;
using google::protobuf::compiler::GeneratorContext;
using google::protobuf::io::Printer;
using google::protobuf::io::ZeroCopyOutputStream;

struct ProtoExtrasGeneratorOptions {
  bool generate_to_value_serialization;
  bool generate_stream_operator;
  bool protobuf_full_support;
};

void FieldToValueFunction(const FieldDescriptor& field, Printer* printer) {
  using enum FieldDescriptor::Type;
  auto conversion_function = [&]() -> std::string {
    switch (field.type()) {
      case TYPE_DOUBLE:
      case TYPE_FLOAT:
        return "static_cast<double>";
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
        return "::proto_extras::ToNumericTypeForValue";
      case TYPE_BOOL:
        return "static_cast<bool>";
      case TYPE_STRING:
        return "static_cast<std::string>";
      case TYPE_BYTES:
        return "base::Base64Encode";
      case TYPE_ENUM:
        return base::StrCat(
            {google::protobuf::compiler::cpp::QualifiedClassName(
                 field.enum_type()),
             "_Name"});
      case TYPE_MESSAGE:
      case TYPE_GROUP:
        // The Serialize function for the message is in the namespace of the
        // nested message itself.
        return base::StrCat(
            {google::protobuf::compiler::cpp::Namespace(field.message_type()),
             "::Serialize"});
    }
    NOTREACHED();
  };
  printer->Print(conversion_function());
}

void CreateSerializationDefinitions(
    const Descriptor& message,
    Printer* printer,
    const ProtoExtrasGeneratorOptions& options) {
  printer->Emit(
      {{"message_type", google::protobuf::compiler::cpp::ClassName(&message)},
       {"serialize_fields",
        [&]() {
          for (int j = 0; j < message.field_count(); j++) {
            const FieldDescriptor& field = *message.field(j);
            std::string field_name(field.lowercase_name());

            auto field_to_value = [&]() {
              FieldToValueFunction(field, printer);
            };
            if (field.is_repeated()) {
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
            } else if (field.has_presence()) {
              printer->Emit({{"field_name", field_name},
                             {"field_to_value", field_to_value}},
                            R"(
  if (message.has_$field_name$()) {
    dict.Set("$field_name$", $field_to_value$(message.$field_name$()));
  }
)");
            } else if (field.type() == FieldDescriptor::Type::TYPE_STRING ||
                       field.type() == FieldDescriptor::Type::TYPE_BYTES) {
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
base::DictValue Serialize(const $message_type$& message) {
  base::DictValue dict;
  if (!message.unknown_fields().empty()) {
    ::proto_extras::SerializeUnknownFields(message, dict);
  }
  $serialize_fields$
  return dict;
}
)");
}

class ProtoExtrasGenerator : public google::protobuf::compiler::CodeGenerator {
 public:
  ProtoExtrasGenerator() = default;
  ~ProtoExtrasGenerator() override = default;

  bool Generate(const FileDescriptor* file,
                const std::string& options,  // Options from build system
                GeneratorContext* context,
                std::string* error) const override {
    CHECK(file);

    ProtoExtrasGeneratorOptions generator_options{
        .generate_to_value_serialization =
            !base::Contains(options, "omit_to_value_serialization"),
        .generate_stream_operator =
            !base::Contains(options, "omit_stream_operators"),
        .protobuf_full_support =
            base::Contains(options, "protobuf_full_support"),
    };
    CHECK(generator_options.generate_to_value_serialization ||
          generator_options.generate_stream_operator);

    base::FilePath proto_file_path = base::FilePath::FromASCII(file->name());
    base::FilePath h_file_path =
        proto_file_path.ReplaceExtension(FILE_PATH_LITERAL("extras.h"));
    base::FilePath cc_file_path =
        proto_file_path.ReplaceExtension(FILE_PATH_LITERAL("extras.cc"));

    const std::unique_ptr<ZeroCopyOutputStream> h_stream(
        context->Open(h_file_path.AsUTF8Unsafe()));
    const std::unique_ptr<ZeroCopyOutputStream> cc_stream(
        context->Open(cc_file_path.AsUTF8Unsafe()));

    Printer h_printer(h_stream.get(), Printer::Options{'$', nullptr});
    Printer cc_printer(cc_stream.get(), Printer::Options{'$', nullptr});

    std::string include_guard =
        base::ToUpperASCII(h_file_path.AsUTF8Unsafe()) + "_";
    CHECK(base::ReplaceChars(include_guard, ".-/\\", "_", &include_guard));

    h_printer.Emit(
        {
            {"include_guard", include_guard},
            {"proto_file_path", proto_file_path.AsUTF8Unsafe()},
            {"includes",
             [&] {
               if (generator_options.generate_stream_operator) {
                 h_printer.Print("#include <iosfwd>\n\n");
               }
               h_printer.Print(
                   "#include \"$f$\"\n", "f",
                   proto_file_path.ReplaceExtension(FILE_PATH_LITERAL("pb.h"))
                       .AsUTF8Unsafe());
             }},
            {"function_declarations",
             [&] {
               google::protobuf::compiler::cpp::NamespaceOpener ns(
                   google::protobuf::compiler::cpp::Namespace(file),
                   &h_printer);
               for (int i = 0; i < file->message_type_count(); i++) {
                 PrintFunctionDeclarations(*file->message_type(i), &h_printer,
                                           error, generator_options);
               }
             }},
        },
        R"(// Generated by the proto_to_extras plugin.  DO NOT EDIT!
// source: $proto_file_path$

#ifndef $include_guard$
#define $include_guard$

$includes$

namespace base {
class DictValue;
}  // namespace base

$function_declarations$

#endif  // $include_guard$
)");

    // Determine the #includes for the implementation file.
    absl::flat_hash_set<std::string> impl_system_inclues;
    // Always have the header and pb.h for the message.
    absl::flat_hash_set<std::string> impl_user_includes = {
        h_file_path.AsUTF8Unsafe(),
        proto_file_path.ReplaceExtension(FILE_PATH_LITERAL("pb.h"))
            .AsUTF8Unsafe(),
    };
    if (generator_options.generate_stream_operator) {
      impl_system_inclues.insert("<ostream>");
    }
    if (generator_options.generate_to_value_serialization) {
      impl_user_includes.insert({"base/base64.h", "base/values.h",
                                 "components/proto_extras/proto_extras_lib.h"});
    }
    for (int i = 0; i < file->dependency_count(); i++) {
      base::FilePath dependency_proto_file_path =
          base::FilePath::FromASCII(file->dependency(i)->name());
      impl_user_includes.insert(
          dependency_proto_file_path
              .ReplaceExtension(FILE_PATH_LITERAL("extras.h"))
              .AsUTF8Unsafe());
    }
    if (generator_options.protobuf_full_support) {
      impl_user_includes.insert(
          "components/proto_extras/protobuf_full_support.h");
    }
    cc_printer.Emit(
        {
            {"proto_file_path", proto_file_path.AsUTF8Unsafe()},
            {"includes",
             [&] {
               for (const auto& include : impl_system_inclues) {
                 cc_printer.Print("#include $f$\n", "f", include);
               }
               for (const auto& include : impl_user_includes) {
                 cc_printer.Print("#include \"$f$\"\n", "f", include);
               }
             }},
            {"function_definitions",
             [&] {
               google::protobuf::compiler::cpp::NamespaceOpener ns(
                   google::protobuf::compiler::cpp::Namespace(file),
                   &cc_printer);
               for (int i = 0; i < file->message_type_count(); i++) {
                 PrintFunctionDefinitions(*file->message_type(i), &cc_printer,
                                          error, generator_options);
               }
             }},
        },
        R"(// Generated by the proto_to_extras plugin.  DO NOT EDIT!
// source: $proto_file_path$

$includes$

$function_definitions$
)");
    return true;
  }

  bool PrintFunctionDeclarations(
      const Descriptor& message,
      Printer* printer,
      std::string* error,
      const ProtoExtrasGeneratorOptions& options) const {
    std::string message_type =
        google::protobuf::compiler::cpp::ClassName(&message);
    if (options.generate_to_value_serialization) {
      printer->Print("base::DictValue Serialize(const $m$& message);\n", "m",
                     message_type);
    }
    if (options.generate_stream_operator) {
      printer->Print(
          "std::ostream& operator<<(std::ostream& out, const "
          "$m$& message);\n",
          "m", message_type);
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
    if (options.generate_to_value_serialization) {
      CreateSerializationDefinitions(message, printer, options);
    }
    if (options.generate_stream_operator) {
      std::string message_type =
          google::protobuf::compiler::cpp::ClassName(&message);
      printer->Emit({{"message_type", message_type}},
                    R"(
std::ostream& operator<<(std::ostream& out, const $message_type$& message) {
  return out << Serialize(message).DebugString();
}
)");
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

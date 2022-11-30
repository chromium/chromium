// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_LOGGING_MESSAGE_BUILDER_H_
#define CHROME_CHROME_CLEANER_LOGGING_MESSAGE_BUILDER_H_

#include <string>

#include "base/strings/string_piece.h"

namespace chrome_cleaner {

// Provides a fluent interface for building messages to be presented to the
// user with functions to add values of basic types and common line patterns.
// The class also provides an interface for automatically increasing the
// indentation level and decrease it back when the current scope ends.
//
// Usage example:
//   MessageBuilder builder;
//   builder.AddHeaderLine(L"Main header");
//   {
//     MessageBuilder::ScopedIndent scoped_indent(&builder);
//     builder
//         .AddFieldValueLine(L"WString field", L"abc")
//         .AddFieldValueLine(L"Int field", 10)
//         .AddFieldValueLine(L"String field", "xyz");
//     {
//       MessageBuilder::ScopedIndent scoped_indent(&builder);
//       builder.Add(L"abc ", "xyz ", 10).NewLine();
//       builder.AddFieldValueLine(L"Another field", L"pqr")

//       MessageBuilder::ScopedIndent scoped_indent_2(&builder);
//       builder.Add(1, L" ", 2, L" ", 4).NewLine();
//     }
//     builder.AddFieldValueLine(L"Last field", L"xyz")
//   }
//
// At the end, builder.content() will contain (| represents the start of the
// line):
//     |Main header:
//     |\tWString field: abc
//     |\tInt field: 10
//     |\tString field: xyz
//     |\t\tabc xyz 10
//     |\t\tAnother field: pqr
//     |\t\t\t1 2 4
//     |\tLast field: xyz
//
// Note: scoped indentation can also be introduced by the following idiom:
//     auto scoped_indent = builder.Indent();
class MessageBuilder {
 public:
  // Increases the indentation level for |builder| in the current scope:
  //   - on construction, this object will indentation level is increased by 1,
  //     so all new lines will start with an additional tab;
  //   - on destruction, indentation level is restored to its previous value.
  // For convenience, an EOL symbol is appended to the result string whenever
  // the indentation level changes and the last appended character is not EOL.
  class ScopedIndent {
   public:
    explicit ScopedIndent(MessageBuilder* builder);
    ScopedIndent(ScopedIndent&& other);

    ScopedIndent(const ScopedIndent&) = delete;
    ScopedIndent& operator=(const ScopedIndent&) = delete;

    ~ScopedIndent();

    ScopedIndent& operator=(ScopedIndent&& other);

   private:
    MessageBuilder* builder_;
  };

  MessageBuilder() = default;

  MessageBuilder(const MessageBuilder&) = delete;
  MessageBuilder& operator=(const MessageBuilder&) = delete;

  // Appends an EOL character to the result string.
  MessageBuilder& NewLine();

  // Appends a list of values to the result string.
  template <typename... Values>
  MessageBuilder& Add(const Values&... values) {
    IndentIfNewLine();
    AddInternal({MessageItem(values)...});
    return *this;
  }

  // Appends a list of values to the result string and then appends an EOL.
  // Equivalent to:
  //   Add(...).NewLine()
  template <typename... Values>
  MessageBuilder& AddLine(const Values&... values) {
    Add(values...).NewLine();
    return *this;
  }

  // Adds a new line with |title| indented with |indentation_level| tabs.
  // Equivalent to:
  //   Add(title, ":").NewLine()
  MessageBuilder& AddHeaderLine(base::WStringPiece title);

  // AddFieldValueLine adds a new line for a pair (|field_name|, |value|)
  // indented with |indentation_level| tabs.
  // Equivalent to:
  //   Add(field_name, ": ", value).NewLine()
  template <typename Value>
  MessageBuilder& AddFieldValueLine(base::WStringPiece field_name,
                                    const Value& value) {
    Add(field_name, L": ", value).NewLine();
    return *this;
  }

  MessageBuilder::ScopedIndent Indent();

  std::wstring content() const { return content_; }

 protected:
  // Updates the current indentation level and appends a L'\n' if it's not the
  // last symbol appended to the result string.
  void IncreaseIdentationLevel();
  void DecreaseIdentationLevel();

 private:
  // Internal representation of a value that can be appended to the result
  // string, so that values of different types can be added to the initializer
  // list in AddInternal().
  class MessageItem {
   public:
    explicit MessageItem(base::WStringPiece value);
    explicit MessageItem(base::StringPiece value);
    explicit MessageItem(int value);

    const std::wstring& value() const { return value_; }

   private:
    std::wstring value_;
  };

  void AddInternal(std::initializer_list<MessageItem> values);
  void IndentIfNewLine();

  std::wstring content_;
  int indentation_level_ = 0;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_LOGGING_MESSAGE_BUILDER_H_

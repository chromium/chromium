// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MODEL_LOCALIZED_TEXT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MODEL_LOCALIZED_TEXT_H_

#include <optional>
#include <string>

#include "base/values.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"

namespace web_app {

// Represents a localized text object with value, language, and text direction
// attributes. See: https://www.w3.org/TR/appmanifest/#localizing-text-values
class LocalizedText {
 public:
  LocalizedText();

  // Creates a LocalizedText with the specified value and optional language and
  // text direction. `value` must not be empty. To create an empty
  // LocalizedText, use the default constructor LocalizedText().
  LocalizedText(std::u16string value,
                std::optional<std::u16string> lang,
                std::optional<blink::mojom::Manifest_TextDirection> dir);

  LocalizedText(const LocalizedText&);
  LocalizedText(LocalizedText&&);
  LocalizedText& operator=(const LocalizedText&);
  LocalizedText& operator=(LocalizedText&&);

  // Implicit assignment from string types. This allows code like:
  // localized_text = u"new value";
  // Template to avoid ambiguity with multiple copy/move assignment operators.
  template <typename T>
    requires(std::assignable_from<std::u16string&, T &&>)
  LocalizedText& operator=(T&& value) {
    value_ = std::forward<T>(value);
    lang_.reset();
    dir_.reset();
    return *this;
  }

  bool operator==(const LocalizedText&) const = default;

  // Allow comparison with string types.
  bool operator==(std::u16string_view other) const { return value_ == other; }

  ~LocalizedText();

  // Returns true if value is empty and optional fields are not populated.
  bool empty() const;

  // Returns a debug representation as a base::Value for logging.
  base::Value AsDebugValue() const;

  const std::u16string& value() const { return value_; }
  const std::optional<std::u16string>& lang() const { return lang_; }
  const std::optional<blink::mojom::Manifest_TextDirection>& dir() const {
    return dir_;
  }

 private:
  std::u16string value_;
  std::optional<std::u16string> lang_;
  std::optional<blink::mojom::Manifest_TextDirection> dir_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MODEL_LOCALIZED_TEXT_H_

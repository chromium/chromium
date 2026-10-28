// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_CUSTOM_FORM_CONTROL_SCRIPT_INDEX_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_CUSTOM_FORM_CONTROL_SCRIPT_INDEX_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/sequence_checker.h"

class GURL;

namespace re2 {
class RE2;
}  // namespace re2

namespace autofill {

// The maximum size of a custom form control script.
inline constexpr size_t kMaxCustomFormControlScriptSizeBytes = 64 * 1024;

// Maps HTTP/HTTPS page URLs to custom form control scripts. There is one index
// per browser process, owned by the embedder. The entries come from a local
// directory given on the command line: the files are read and parsed on a
// background sequence, and the result is passed to `SetEntries()` on the UI
// sequence.
//
// The index isn't synchronized: it must be created and used on the same
// sequence (the browser UI sequence).
class CustomFormControlScriptIndex final {
 public:
  // Maps a `host` (with a registrable domain, e.g. `example.com` or
  // `shop.example.com`, matching that host and its subdomains) and an optional
  // RE2 `url_filter` to `script` contents. `url_filter` is searched anywhere in
  // `url.PathForRequest()` (canonical path plus optional `?query`, excluding
  // `#fragment`); use `^`/`$` to anchor it. An empty `url_filter` matches all
  // URLs on the host.
  struct Entry {
    std::string host;
    std::string url_filter;
    std::string script;
  };

  CustomFormControlScriptIndex();
  CustomFormControlScriptIndex(const CustomFormControlScriptIndex&) = delete;
  CustomFormControlScriptIndex& operator=(const CustomFormControlScriptIndex&) =
      delete;
  ~CustomFormControlScriptIndex();

  // Validates, compiles, and replaces the current entries. Entries with an
  // uncanonicalizable or public-suffix-only host, an invalid `url_filter`
  // regex, or an empty/oversized `script` are dropped. Entry order is
  // preserved (the first matching entry wins).
  void SetEntries(std::vector<Entry> entries);

  // Returns a copy of the script of the first entry matching `url` (HTTP/HTTPS
  // only), or std::nullopt if none matches.
  std::optional<std::string> FindScript(const GURL& url) const;

 private:
  struct CompiledEntry {
    CompiledEntry(std::string host,
                  std::unique_ptr<const re2::RE2> filter,
                  std::string script);
    CompiledEntry(CompiledEntry&&);
    CompiledEntry& operator=(CompiledEntry&&);
    ~CompiledEntry();

    std::string host;
    std::unique_ptr<const re2::RE2> filter;
    std::string script;
  };

  std::vector<CompiledEntry> entries_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_CUSTOM_FORM_CONTROL_SCRIPT_INDEX_H_

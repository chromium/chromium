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

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace re2 {
class RE2;
}  // namespace re2

namespace autofill {

// The maximum size of a custom form control script.
inline constexpr size_t kMaxCustomFormControlScriptSizeBytes = 64 * 1024;

// Maps HTTP/HTTPS page URLs to custom form control scripts. There is one index
// per browser process: in Chrome, it is owned by `GlobalFeatures`, which
// creates it at startup if `features::kAutofillCustomFormControlScripts` is
// enabled and `--autofill-custom-form-control-scripts-dir` is set. The entries
// come from that local directory (see `InitializeFromDirectory()`).
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

  // Asynchronously replaces the entries with the ones listed in
  // `directory`/index.json, a JSON list of objects of the form:
  //
  //   [{"host": "example.com", "url_filter": "/checkout",
  //     "script_path": "example_com.js"}]
  //
  // `host` and `url_filter` are as in `Entry`; `url_filter` is optional.
  // `script_path` is the path of the script file, relative to `directory`.
  //
  // The files are read on a background sequence, and the entries are then
  // passed to `SetEntries()`. If index.json can't be read or isn't a JSON list,
  // the current entries are kept. Either way, `is_initialized()` returns true
  // afterwards. Items with missing or non-string fields, or with a
  // `script_path` that is absolute or contains `..`, are skipped, as are
  // scripts that can't be read or exceed
  // `kMaxCustomFormControlScriptSizeBytes`.
  void InitializeFromDirectory(const base::FilePath& directory);

  // Returns true once the most recent `InitializeFromDirectory()` call has
  // finished reading the directory, whether or not it succeeded.
  bool is_initialized() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_initialized_;
  }

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
  bool is_initialized_ = false;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CustomFormControlScriptIndex> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_CUSTOM_FORM_CONTROL_SCRIPT_INDEX_H_

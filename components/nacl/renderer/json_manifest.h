// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_JSON_MANIFEST_H_
#define COMPONENTS_NACL_RENDERER_JSON_MANIFEST_H_

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/values.h"
#include "components/nacl/renderer/ppb_nacl_private.h"

namespace nacl {
class JsonManifest;
struct NaClResourcePrefetchRequest;

class JsonManifest {
 public:
  struct ErrorInfo {
    PP_NaClError error;
    std::string string;
  };

  JsonManifest(const std::string& manifest_base_url,
               const std::string& sandbox_isa,
               bool pnacl_debug);
  ~JsonManifest();

  // Initialize the manifest object for use by later lookups. Returns
  // true if the manifest parses correctly and matches the schema.
  bool Init(const std::string& json_manifest, ErrorInfo* error_info);

  // Gets the full program URL for the current sandbox ISA from the
  // manifest file.
  bool GetProgramURL(std::string* full_url,
                     PP_PNaClOptions* pnacl_options,
                     ErrorInfo* error_info) const;

  // Gets all the keys and their URLs in the "files" section that are
  // prefetchable.
  void GetPrefetchableFiles(
      std::vector<NaClResourcePrefetchRequest>* out_files) const;

  // Resolves a key from the "files" section to a fully resolved URL,
  // i.e., relative URL values are fully expanded relative to the
  // manifest's URL (via ResolveURL).
  // If there was an error, details are reported via error_info.
  bool ResolveKey(const std::string& key,
                  std::string* full_url,
                  PP_PNaClOptions* pnacl_options) const;

 private:
  bool MatchesSchema(ErrorInfo* error_info);
  bool GetKeyUrl(const base::Value::Dict& dictionary,
                 const std::string& key,
                 std::string* full_url,
                 PP_PNaClOptions* pnacl_options) const;
  bool GetURLFromISADictionary(const base::Value::Dict& parent_dictionary,
                               const std::string& parent_key,
                               std::string* url,
                               PP_PNaClOptions* pnacl_options,
                               ErrorInfo* error_info) const;

  std::string manifest_base_url_;
  std::string sandbox_isa_;
  bool pnacl_debug_;

  // The dictionary of manifest information parsed in Init().
  base::Value::Dict dictionary_;
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_JSON_MANIFEST_H_

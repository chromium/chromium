// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_LOAD_SCRIPT_INJECTOR_BROWSER_ON_LOAD_SCRIPT_INJECTOR_HOST_H_
#define COMPONENTS_ON_LOAD_SCRIPT_INJECTOR_BROWSER_ON_LOAD_SCRIPT_INJECTOR_HOST_H_

#include <map>
#include <string_view>
#include <vector>

#include "base/memory/read_only_shared_memory_region.h"
#include "components/on_load_script_injector/export.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace on_load_script_injector {

class ON_LOAD_SCRIPT_INJECTOR_EXPORT OriginScopedScript {
 public:
  OriginScopedScript();
  OriginScopedScript(std::vector<url::Origin> origins,
                     base::ReadOnlySharedMemoryRegion script);
  OriginScopedScript& operator=(OriginScopedScript&& other);
  ~OriginScopedScript();

  const std::vector<url::Origin>& origins() const { return origins_; }
  const base::ReadOnlySharedMemoryRegion& script() const { return script_; }

 private:
  std::vector<url::Origin> origins_;

  // A shared memory buffer containing the script, encoded as UTF16.
  base::ReadOnlySharedMemoryRegion script_;
};

// Manages the set of scripts to be injected into document just prior to
// document load.
template <typename ScriptId>
class ON_LOAD_SCRIPT_INJECTOR_EXPORT OnLoadScriptInjectorHost {
 public:
  OnLoadScriptInjectorHost();
  ~OnLoadScriptInjectorHost();

  OnLoadScriptInjectorHost(const OnLoadScriptInjectorHost&) = delete;
  OnLoadScriptInjectorHost& operator=(const OnLoadScriptInjectorHost&) = delete;

  // Adds a |script| to be injected on pages whose URL's origin matches at least
  // one entry of |origins_to_inject|.
  // Scripts will be loaded in the order they are added.
  // If a script with |id| already exists, it will be replaced with the original
  // sequence position preserved.
  // All entries of |origins_to_inject| must be valid/not opaque.
  void AddScript(ScriptId id,
                 std::vector<url::Origin> origins_to_inject,
                 std::string_view script);

  // Same as AddScript(), except that scripts are injected for all pages.
  void AddScriptForAllOrigins(ScriptId id, std::string_view script);

  // Removes the script |id|.
  void RemoveScript(ScriptId id);

  // Injects the scripts associated with the origin of |url| into the document
  // hosted by |render_frame_host|.
  void InjectScriptsForURL(const GURL& url,
                           content::RenderFrameHost* render_frame_host);

 private:
  bool IsUrlMatchedByOriginList(
      const GURL& url,
      const std::vector<url::Origin>& allowed_origins);

  // An opaque Origin that, when specified, allows script injection on all URLs
  // regardless of origin.
  const url::Origin kMatchAllOrigins;

  std::map<ScriptId, OriginScopedScript> before_load_scripts_;
  std::vector<ScriptId> before_load_scripts_order_;
};

}  // namespace on_load_script_injector

#endif  // COMPONENTS_ON_LOAD_SCRIPT_INJECTOR_BROWSER_ON_LOAD_SCRIPT_INJECTOR_HOST_H_

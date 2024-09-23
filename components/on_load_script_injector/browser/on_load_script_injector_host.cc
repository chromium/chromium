// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_load_script_injector/browser/on_load_script_injector_host.h"

#include <string_view>
#include <utility>

#include "base/numerics/safe_math.h"
#include "base/strings/utf_string_conversions.h"
#include "components/on_load_script_injector/on_load_script_injector.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/gurl.h"

namespace on_load_script_injector {

OriginScopedScript::OriginScopedScript() = default;

OriginScopedScript::OriginScopedScript(std::vector<url::Origin> origins,
                                       base::ReadOnlySharedMemoryRegion script)
    : origins_(std::move(origins)), script_(std::move(script)) {}

OriginScopedScript& OriginScopedScript::operator=(OriginScopedScript&& other) {
  origins_ = std::move(other.origins_);
  script_ = std::move(other.script_);
  return *this;
}

OriginScopedScript::~OriginScopedScript() = default;

template <typename ScriptId>
OnLoadScriptInjectorHost<ScriptId>::OnLoadScriptInjectorHost() = default;
template <typename ScriptId>
OnLoadScriptInjectorHost<ScriptId>::~OnLoadScriptInjectorHost() = default;

template <typename ScriptId>
void OnLoadScriptInjectorHost<ScriptId>::AddScript(
    ScriptId id,
    std::vector<url::Origin> origins_to_inject,
    std::string_view script) {
  // If there is no script with the identifier |id|, then create a place for
  // it at the end of the injection sequence.
  if (before_load_scripts_.find(id) == before_load_scripts_.end())
    before_load_scripts_order_.push_back(id);

  // Convert script to UTF-16.
  std::u16string script_utf16 = base::UTF8ToUTF16(script);
  size_t script_utf16_size =
      (base::CheckedNumeric<size_t>(script_utf16.size()) * sizeof(char16_t))
          .ValueOrDie();
  base::WritableSharedMemoryRegion script_shared_memory =
      base::WritableSharedMemoryRegion::Create(script_utf16_size);
  memcpy(script_shared_memory.Map().memory(), script_utf16.data(),
         script_utf16_size);

  base::ReadOnlySharedMemoryRegion script_shared_memory_readonly =
      base::WritableSharedMemoryRegion::ConvertToReadOnly(
          std::move(script_shared_memory));
  CHECK(script_shared_memory_readonly.IsValid());

  before_load_scripts_[id] = OriginScopedScript(
      origins_to_inject, std::move(script_shared_memory_readonly));
}

template <typename ScriptId>
void OnLoadScriptInjectorHost<ScriptId>::AddScriptForAllOrigins(
    ScriptId id,
    std::string_view script) {
  AddScript(id, {kMatchAllOrigins}, script);
}

template <typename ScriptId>
void OnLoadScriptInjectorHost<ScriptId>::RemoveScript(ScriptId id) {
  before_load_scripts_.erase(id);

  for (auto script_id_iter = before_load_scripts_order_.begin();
       script_id_iter != before_load_scripts_order_.end(); ++script_id_iter) {
    if (*script_id_iter == id) {
      before_load_scripts_order_.erase(script_id_iter);
      return;
    }
  }

  LOG(WARNING) << "Ignoring attempt to remove unknown OnLoad script: " << id;
}

template <typename ScriptId>
void OnLoadScriptInjectorHost<ScriptId>::InjectScriptsForURL(
    const GURL& url,
    content::RenderFrameHost* render_frame_host) {
  DCHECK(url.is_valid());

  mojo::AssociatedRemote<mojom::OnLoadScriptInjector> injector;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&injector);

  injector->ClearOnLoadScripts();

  if (before_load_scripts_.empty())
    return;

  // Provision the renderer's ScriptInjector with the scripts associated with
  // |url|.
  for (ScriptId script_id : before_load_scripts_order_) {
    const OriginScopedScript& script = before_load_scripts_[script_id];
    if (IsUrlMatchedByOriginList(url, script.origins()))
      injector->AddOnLoadScript(script.script().Duplicate());
  }
}

template <typename ScriptId>
bool OnLoadScriptInjectorHost<ScriptId>::IsUrlMatchedByOriginList(
    const GURL& url,
    const std::vector<url::Origin>& allowed_origins) {
  for (const url::Origin& allowed_origin : allowed_origins) {
    if (allowed_origin == kMatchAllOrigins)
      return true;

    DCHECK(!allowed_origin.opaque());
    if (allowed_origin.IsSameOriginWith(url))
      return true;
  }

  return false;
}

template class OnLoadScriptInjectorHost<std::string>;
template class OnLoadScriptInjectorHost<uint64_t>;

}  // namespace on_load_script_injector

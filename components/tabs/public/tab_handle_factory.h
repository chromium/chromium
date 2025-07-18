// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_TAB_HANDLE_FACTORY_H_
#define COMPONENTS_TABS_PUBLIC_TAB_HANDLE_FACTORY_H_

#include <map>
#include <optional>

#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "components/tabs/public/supports_handles.h"

namespace tabs {

class TabInterface;
class SupportsTabHandles;

DECLARE_BASE_HANDLE_FACTORY(TabInterface);

// A specialized handle factory for `TabInterface` that also maintains a mapping
// between handle values and SessionIDs.
// Ideally, session IDs could be interchangeable with `TabInterface` handles
// without a mapping, but this is infeasible until we can guarantee there is one
// `WebContents` per `TabInterface`.
class SessionMappedTabHandleFactory final
    : public TabInterfaceHandleFactoryBase {
 public:
  static SessionMappedTabHandleFactory& GetInstance();

  int32_t GetHandleForSessionId(int32_t session_id) const;
  std::optional<int32_t> GetSessionIdForHandle(int32_t handle) const;

  void SetSessionIdForHandle(base::PassKey<SupportsTabHandles>,
                             int32_t handle,
                             int32_t session_id);
  // Clears all mappings associated with the handle.
  void ClearHandleMappings(base::PassKey<SupportsTabHandles>, int32_t handle);

 private:
  friend class ::base::NoDestructor<SessionMappedTabHandleFactory>;
  SessionMappedTabHandleFactory();
  ~SessionMappedTabHandleFactory() override;

  // TabInterfaceHandleFactoryBase
  void OnHandleFreed(int32_t handle_value) override;

  std::map<int32_t, int32_t> session_id_to_handle_value_
      GUARDED_BY_CONTEXT(sequence());
  std::map<int32_t, int32_t> handle_value_to_session_id_
      GUARDED_BY_CONTEXT(sequence());
};

class SupportsTabHandles
    : public SupportsHandles<SessionMappedTabHandleFactory> {
 public:
  ~SupportsTabHandles() override = default;

 protected:
  void SetSessionId(int32_t sesion_id);

  // Resets the mapped session IDs.
  // This is necessary as long as a tab may have multiple web contents
  // throughout its lifetime.
  void ClearSessionId();
};

}  // namespace tabs

#endif  // COMPONENTS_TABS_PUBLIC_TAB_HANDLE_FACTORY_H_

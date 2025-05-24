// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DATA_SOURCE_DELEGATE_H_
#define COMPONENTS_EXO_DATA_SOURCE_DELEGATE_H_

#include <optional>
#include <string>

#include "base/files/scoped_file.h"
#include "components/exo/data_device.h"

namespace exo {

class DataSource;
class SecurityDelegate;

// Handles events on data devices in context-specific ways.
class DataSourceDelegate {
 public:
  // Called at the top of the data device's destructor, to give observers a
  // chance to remove themselves.
  virtual void OnDataSourceDestroying(DataSource* source) = 0;

  // Called when a client accepts a |mime_type|.
  virtual void OnTarget(const std::optional<std::string>& mime_type) = 0;

  // Called when the data is requested.
  virtual void OnSend(const std::string& mime_type, base::ScopedFD fd) = 0;

  // Called when selection or drag and drop operation was cancelled.
  virtual void OnCancelled() = 0;

  // Called when a user performes drop operation.
  virtual void OnDndDropPerformed() = 0;

  // Called when the drag and drop operation completes and compositor stop using
  // the data source.
  virtual void OnDndFinished() = 0;

  // Called when the compositor selects one drag and drop action.
  virtual void OnAction(DndAction dnd_action) = 0;

  // This should return true if |surface| is the source of this data source.
  // E.g. the surface is owned by the same client as the data source.
  virtual bool CanAcceptDataEventsForSurface(Surface* surface) const = 0;

  // Returns the server's SecurityDelegate.
  virtual SecurityDelegate* GetSecurityDelegate() const = 0;

 protected:
  virtual ~DataSourceDelegate() = default;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DATA_SOURCE_DELEGATE_H_

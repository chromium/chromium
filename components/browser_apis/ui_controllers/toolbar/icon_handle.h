// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_TOOLBAR_ICON_HANDLE_H_
#define COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_TOOLBAR_ICON_HANDLE_H_

#include <cstdint>

#include "base/memory/ref_counted.h"
#include "base/types/id_type.h"

namespace toolbar_ui_api {

struct IconHandleIdTag {};

// ID that will never be assigned to an icon, and that stands for an empty
// icon.
constexpr uint64_t kNullIconHandleId = 0;

// Representation of a handle for an icon, suitable for being sent over
// mojo --- to send an IconHandle, its IconHandleId is sent.
using IconHandleId = base::IdType<IconHandleIdTag, uint64_t, kNullIconHandleId>;

// An abstraction of a handle to an icon to be sent to WebUI. Helps keep the
// icon available.
class IconHandle {
 public:
  // Implementations of this API will creates these to construct IconHandles
  // to those they track.
  class Provider : public base::RefCounted<Provider> {
   public:
    virtual IconHandleId HandleId() = 0;

   protected:
    friend class base::RefCounted<Provider>;
    Provider() = default;
    virtual ~Provider() = default;
  };

  // Default constructed one represent the empty icon (and gets encoded as
  // kNullIconHandleId).
  IconHandle();
  explicit IconHandle(scoped_refptr<Provider>);
  IconHandle(const IconHandle& other);
  IconHandle(IconHandle&& other);
  ~IconHandle();

  IconHandle& operator=(const IconHandle& other);
  IconHandle& operator=(IconHandle&& other);

  // Returns the ID that should be sent over mojo for this IconHandle.
  IconHandleId HandleId() const;
  bool is_null() const { return !provider_; }

  friend bool operator==(const IconHandle&, const IconHandle&) = default;

 private:
  scoped_refptr<Provider> provider_;
};

}  // namespace toolbar_ui_api

#endif  // COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_TOOLBAR_ICON_HANDLE_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_MEDIA_APP_EVENTS_PROXY_H_
#define CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_MEDIA_APP_EVENTS_PROXY_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "base/unguessable_token.h"

namespace gfx {
class Rect;
}

namespace chromeos {

// An interface serves as events proxy from media app to mahi system.
class COMPONENT_EXPORT(MAHI_PUBLIC_CPP) MahiMediaAppEventsProxy {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPdfGetFocus(const base::UnguessableToken client_id) {}
    // The context menu here is not the system context menu but the one
    // implemented by the media app.
    virtual void OnPdfContextMenuShown(const gfx::Rect& anchor) {}
    virtual void OnPdfContextMenuHide() {}
    virtual void OnPdfClosed(const base::UnguessableToken client_id) {}
  };

  MahiMediaAppEventsProxy(const MahiMediaAppEventsProxy&) = delete;
  MahiMediaAppEventsProxy& operator=(const MahiMediaAppEventsProxy&) = delete;

  virtual ~MahiMediaAppEventsProxy();

  static MahiMediaAppEventsProxy* Get();

  virtual void OnPdfGetFocus(const base::UnguessableToken client_id) = 0;
  virtual void OnPdfContextMenuShown(const base::UnguessableToken client_id,
                                     const gfx::Rect& anchor) = 0;
  virtual void OnPdfContextMenuHide() = 0;
  virtual void OnPdfClosed(const base::UnguessableToken client_id) = 0;
  virtual void AddObserver(Observer*) = 0;
  virtual void RemoveObserver(Observer*) = 0;

 protected:
  MahiMediaAppEventsProxy();
};

// A scoped object that set the global instance of
// `chromeos::MahiMediaAppEventsProxy::Get()` to the provided object pointer.
// The real instance will be restored when this scoped object is destructed.
// This class is used in testing and mocking.
class COMPONENT_EXPORT(MAHI_PUBLIC_CPP) ScopedMahiMediaAppEventsProxySetter {
 public:
  explicit ScopedMahiMediaAppEventsProxySetter(MahiMediaAppEventsProxy* proxy);
  ScopedMahiMediaAppEventsProxySetter(
      const ScopedMahiMediaAppEventsProxySetter&) = delete;
  ScopedMahiMediaAppEventsProxySetter& operator=(
      const ScopedMahiMediaAppEventsProxySetter&) = delete;
  ~ScopedMahiMediaAppEventsProxySetter();

 private:
  static ScopedMahiMediaAppEventsProxySetter* instance_;

  raw_ptr<MahiMediaAppEventsProxy> real_proxy_instance_ = nullptr;
};

}  // namespace chromeos
#endif  // CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_MEDIA_APP_EVENTS_PROXY_H_

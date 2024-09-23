// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mahi/public/cpp/mahi_media_app_events_proxy.h"

#include "base/notreached.h"

namespace chromeos {

namespace {
MahiMediaAppEventsProxy* g_instance = nullptr;
}  // namespace

// static
MahiMediaAppEventsProxy* MahiMediaAppEventsProxy::Get() {
  return g_instance;
}

MahiMediaAppEventsProxy::MahiMediaAppEventsProxy() {
  DCHECK(!g_instance);
  g_instance = this;
}

MahiMediaAppEventsProxy::~MahiMediaAppEventsProxy() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
ScopedMahiMediaAppEventsProxySetter*
    ScopedMahiMediaAppEventsProxySetter::instance_ = nullptr;

ScopedMahiMediaAppEventsProxySetter::ScopedMahiMediaAppEventsProxySetter(
    MahiMediaAppEventsProxy* proxy) {
  // Only allow one scoped instance at a time.
  if (instance_) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  instance_ = this;

  // Save the real manager instance and replace it with the fake one.
  real_proxy_instance_ = g_instance;
  g_instance = proxy;
}

ScopedMahiMediaAppEventsProxySetter::~ScopedMahiMediaAppEventsProxySetter() {
  if (instance_ != this) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  instance_ = nullptr;

  g_instance = real_proxy_instance_;
  real_proxy_instance_ = nullptr;
}

}  // namespace chromeos

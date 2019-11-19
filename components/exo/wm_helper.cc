// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wm_helper.h"

namespace exo {

namespace {

WMHelper* g_instance = nullptr;

}  // namespace

WMHelper::LifetimeManager::LifetimeManager() = default;

WMHelper::LifetimeManager::~LifetimeManager() {
  for (Observer& observer : observers_)
    observer.OnDestroyed();
}

void WMHelper::LifetimeManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void WMHelper::LifetimeManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

WMHelper::WMHelper() {}

WMHelper::~WMHelper() {}

// static
void WMHelper::SetInstance(WMHelper* helper) {
  DCHECK_NE(!!helper, !!g_instance);
  g_instance = helper;
}

// static
WMHelper* WMHelper::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

// static
bool WMHelper::HasInstance() {
  return !!g_instance;
}

}  // namespace exo

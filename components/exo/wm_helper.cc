// Copyright 2016 The Chromium Authors
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

WMHelper::WMHelper() {
  DCHECK(!g_instance);
  g_instance = this;
}

WMHelper::~WMHelper() {
  DCHECK(g_instance);
  g_instance = nullptr;
}

void WMHelper::AddExoWindowObserver(ExoWindowObserver* observer) {
  exo_window_observers_.AddObserver(observer);
}

void WMHelper::RemoveExoWindowObserver(ExoWindowObserver* observer) {
  exo_window_observers_.RemoveObserver(observer);
}

void WMHelper::AddPowerObserver(PowerObserver* observer) {
  NOTREACHED();
}

void WMHelper::RemovePowerObserver(PowerObserver* observer) {
  NOTREACHED();
}

void WMHelper::NotifyExoWindowCreated(aura::Window* window) {
  for (auto& obs : exo_window_observers_)
    obs.OnExoWindowCreated(window);
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

void WMHelper::RegisterAppPropertyResolver(
    std::unique_ptr<AppPropertyResolver> resolver) {
  resolver_list_.push_back(std::move(resolver));
}

void WMHelper::PopulateAppProperties(
    const AppPropertyResolver::Params& params,
    ui::PropertyHandler& out_properties_container) {
  for (auto& resolver : resolver_list_) {
    resolver->PopulateProperties(params, out_properties_container);
  }
}

}  // namespace exo

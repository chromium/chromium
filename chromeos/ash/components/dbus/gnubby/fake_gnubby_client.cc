// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/gnubby/fake_gnubby_client.h"

namespace ash {

FakeGnubbyClient::FakeGnubbyClient() {}

FakeGnubbyClient::~FakeGnubbyClient() = default;

void FakeGnubbyClient::Init(dbus::Bus* bus) {}

void FakeGnubbyClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeGnubbyClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakeGnubbyClient::SignalPromptUserAuth() {
  calls_++;
  for (auto& observer : observer_list_)
    observer.PromptUserAuth();
}

}  // namespace ash

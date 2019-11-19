// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_gnubby_client.h"

namespace chromeos {

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

}  // namespace chromeos

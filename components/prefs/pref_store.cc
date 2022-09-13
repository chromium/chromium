// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_store.h"

bool PrefStore::HasObservers() const {
  return false;
}

bool PrefStore::IsInitializationComplete() const {
  return true;
}

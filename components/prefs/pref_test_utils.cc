// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_test_utils.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

void WaitForPrefValue(PrefService* pref_service,
                      const std::string& path,
                      const base::Value& value) {
  if (value == pref_service->GetValue(path))
    return;

  base::RunLoop run_loop;
  PrefChangeRegistrar pref_changes;
  pref_changes.Init(pref_service);
  pref_changes.Add(path, base::BindLambdaForTesting([&]() {
                     if (value == pref_service->GetValue(path))
                       run_loop.Quit();
                   }));
  run_loop.Run();
}

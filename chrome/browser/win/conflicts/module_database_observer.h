// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_MODULE_DATABASE_OBSERVER_H_
#define CHROME_BROWSER_WIN_CONFLICTS_MODULE_DATABASE_OBSERVER_H_

struct ModuleInfoKey;
struct ModuleInfoData;

class ModuleDatabaseObserver {
 public:
  // Invoked when a new module is found either by loading into a Chrome process,
  // or by being registered as a shell extension or IME. Only invoked after the
  // module was inspected on disk.
  // Note that this is not invoked multiple times if the module subsequently
  // loads into a different process type.
  virtual void OnNewModuleFound(const ModuleInfoKey& module_key,
                                const ModuleInfoData& module_data) {}

  // Invoked when a registered module (shell extension or IME) that already
  // triggered a OnNewModuleFound() call finally loads into Chrome.
  virtual void OnKnownModuleLoaded(const ModuleInfoKey& module_key,
                                   const ModuleInfoData& module_data) {}

  // Invoked when the ModuleDatabase becomes idle. This means that the
  // ModuleDatabase stopped inspecting modules and it received no new module
  // events in the last 10 seconds.
  virtual void OnModuleDatabaseIdle() {}

 protected:
  virtual ~ModuleDatabaseObserver() = default;
};

class ModuleDatabaseEventSource {
 public:
  virtual void AddObserver(ModuleDatabaseObserver* observer) = 0;
  virtual void RemoveObserver(ModuleDatabaseObserver* observer) = 0;

 protected:
  virtual ~ModuleDatabaseEventSource() = default;
};

#endif  // CHROME_BROWSER_WIN_CONFLICTS_MODULE_DATABASE_OBSERVER_H_

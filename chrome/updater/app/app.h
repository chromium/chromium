// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_APP_H_
#define CHROME_UPDATER_APP_APP_H_

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

// An App is an abstract class used as a main processing mode for the updater.
// Prefer creating non-singleton instances of |App| using |base::MakeRefCounted|
// then use |updater::AppSingletonInstance| only when a singleton instance is
// required by the design. Typically, |App| instances are not singletons but
// there are cases where a singleton is needed, such as the Windows RPC
// server app instance.
class App : public base::RefCountedThreadSafe<App> {
 public:
  // Runs a runloop on the main sequence until `Shutdown()` is called. Returns
  // the exit code for the program.
  [[nodiscard]] int Run();

 protected:
  friend class base::RefCountedThreadSafe<App>;

  App();
  virtual ~App();

  // Triggers program shutdown. Must be called on the main sequence. The program
  // will exit with the specified code.
  void Shutdown(int exit_code);

  virtual UpdaterScope updater_scope() const;

 private:
  // Implementations of App can override this to perform work on the main
  // sequence while blocking is still allowed.
  [[nodiscard]] virtual int Initialize();

  // Called on the main sequence while blocking is allowed and before
  // shutting down the thread pool.
  virtual void Uninitialize() {}

  // Concrete implementations of App can execute their first task in this
  // method. It is called on the main sequence. Blocking is not allowed. It may
  // call Shutdown.
  virtual void FirstTaskRun() = 0;

  // Runs the loop of the main sequence. Blocking is not allowed.
  [[nodiscard]] int RunTasks();

  // A callback that quits the main sequence runloop.
  base::OnceCallback<void(int)> quit_;

  // Indicates the scope of the updater: per-system or per-user.
  const UpdaterScope updater_scope_ = GetUpdaterScope();
};

}  // namespace updater

#endif  // CHROME_UPDATER_APP_APP_H_

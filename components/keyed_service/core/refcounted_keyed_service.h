// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_REFCOUNTED_KEYED_SERVICE_H_
#define COMPONENTS_KEYED_SERVICE_CORE_REFCOUNTED_KEYED_SERVICE_H_

#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "components/keyed_service/core/keyed_service_export.h"

class RefcountedKeyedService;

namespace impl {

struct KEYED_SERVICE_EXPORT RefcountedKeyedServiceTraits {
  static void Destruct(const RefcountedKeyedService* obj);
};

}  // namespace impl

// Base class for refcounted objects that hang off the BrowserContext.
//
// The two pass shutdown described in KeyedService works a bit differently
// because there could be outstanding references on other threads.
// ShutdownOnUIThread() will be called on the UI thread, and then the
// destructor will run when the last reference is dropped, which may or may not
// be after the corresponding BrowserContext has been destroyed.
//
// Optionally, if you initialize your service with the constructor that takes a
// SequencedTaskRunner, your service will be deleted on that sequence. We
// can't use content::DeleteOnThread<> directly because RefcountedKeyedService
// must not depend on //content.
class KEYED_SERVICE_EXPORT RefcountedKeyedService
    : public base::RefCountedThreadSafe<RefcountedKeyedService,
                                        impl::RefcountedKeyedServiceTraits> {
 public:
  RefcountedKeyedService(const RefcountedKeyedService&) = delete;
  RefcountedKeyedService& operator=(const RefcountedKeyedService&) = delete;

  // Unlike KeyedService, ShutdownOnUI is not optional. You must do something
  // to drop references during the first pass Shutdown() because this is the
  // only point where you are guaranteed that something is running on the UI
  // thread. The PKSF framework will ensure that this is only called on the UI
  // thread; you do not need to check for that yourself.
  virtual void ShutdownOnUIThread() = 0;

 protected:
  // If your service does not need to be deleted on a specific sequence, use the
  // default constructor.
  RefcountedKeyedService();

  // If you need your service to be deleted on a specific sequence (for example,
  // you're converting a service that used content::DeleteOnThread<IO>), then
  // use this constructor with a reference to the SequencedTaskRunner (e.g., you
  // can get it from content::Get(UI|IO)ThreadTaskRunner or
  // base::ThreadPool::CreateSequencedTaskRunner).
  explicit RefcountedKeyedService(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // The second pass destruction can happen anywhere unless you specify which
  // sequence this service must be destroyed on by using the second constructor.
  virtual ~RefcountedKeyedService();

 private:
  friend struct impl::RefcountedKeyedServiceTraits;
  friend class base::DeleteHelper<RefcountedKeyedService>;
  friend class base::RefCountedThreadSafe<RefcountedKeyedService,
                                          impl::RefcountedKeyedServiceTraits>;

  // Do we have to delete this object on a specific sequence?
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_REFCOUNTED_KEYED_SERVICE_H_

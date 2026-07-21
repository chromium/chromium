// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_SERVICE_IMPL_H_
#define COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_SERVICE_IMPL_H_

#include "base/observer_list.h"
#include "components/notebooks/public/notebooks_service.h"

namespace notebooks {

// The internal implementation of the NotebooksService.
class NotebooksServiceImpl : public NotebooksService {
 public:
  NotebooksServiceImpl();
  ~NotebooksServiceImpl() override;

  // Disallow copy/assign.
  NotebooksServiceImpl(const NotebooksServiceImpl&) = delete;
  NotebooksServiceImpl& operator=(const NotebooksServiceImpl&) = delete;

  // NotebooksService:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool IsEmptyForTesting() const override;
  bool IsUserEligible() const override;
  bool IsEligibilityLoading() const override;

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_SERVICE_IMPL_H_

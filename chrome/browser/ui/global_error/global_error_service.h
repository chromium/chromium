// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_SERVICE_H_
#define CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_SERVICE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/global_error/global_error_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class GlobalError;

// This service manages a list of errors that are meant to be shown globally.
// If an error applies to an entire profile and not just to a tab then the
// error should be shown using this service. Examples of global errors are:
//   - the previous session crashed for a given profile.
//   - a sync error occurred
class GlobalErrorService : public KeyedService {
 public:
  // Type used to represent the list of currently active errors.
  using GlobalErrorList = std::vector<raw_ptr<GlobalError, VectorExperimental>>;

  // Constructs a GlobalErrorService object.
  GlobalErrorService();

  GlobalErrorService(const GlobalErrorService&) = delete;
  GlobalErrorService& operator=(const GlobalErrorService&) = delete;

  ~GlobalErrorService() override;

  void AddObserver(GlobalErrorObserver* observer);
  void RemoveObserver(GlobalErrorObserver* observer);

  // Adds the given error to the list of global errors and displays it on
  // browser windows. If no browser windows are open then the error is
  // displayed once a browser window is opened.
  void AddGlobalError(std::unique_ptr<GlobalError> error);

  // Hides the given error and removes it from the list of global errors.
  // Ownership is returned to the caller.
  std::unique_ptr<GlobalError> RemoveGlobalError(GlobalError* error);

  // DEPRECATED; DO NOT USE!
  // http://crbug.com/673578
  //
  // These functions allow adding and removing of a GlobalError that is not
  // owned by the GlobalErrorService. In this case, the error's lifetime must
  // exceed that of the GlobalErrorService.
  //
  // Do not add more uses of these functions, and remove existing uses.
  void AddUnownedGlobalError(GlobalError* error);
  void RemoveUnownedGlobalError(GlobalError* error);

  // Gets the error with the given command ID or NULL if no such error exists.
  // This class maintains ownership of the returned error.
  GlobalError* GetGlobalErrorByMenuItemCommandID(int command_id) const;

  // Gets the highest severity error that has a app menu item.
  // Returns NULL if no such error exists.
  GlobalError* GetHighestSeverityGlobalErrorWithAppMenuItem() const;

  // Gets the first error that has a bubble view which hasn't been shown yet.
  // Returns NULL if no such error exists.
  GlobalError* GetFirstGlobalErrorWithBubbleView() const;

  // Gets all errors.
  const GlobalErrorList& errors() { return all_errors_; }

  // Post a notification that a global error has changed and that the error UI
  // should update it self.
  void NotifyErrorsChanged();

 private:
  base::ObserverList<GlobalErrorObserver> observer_list_;

  GlobalErrorList all_errors_;
  std::map<GlobalError*, std::unique_ptr<GlobalError>> owned_errors_;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_SERVICE_H_

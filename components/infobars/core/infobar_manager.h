// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CORE_INFOBAR_MANAGER_H_
#define COMPONENTS_INFOBARS_CORE_INFOBAR_MANAGER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/infobars/core/infobar_delegate.h"

class GURL;
class TestInfoBar;

namespace infobars {

class InfoBar;

// Provides access to creating, removing and enumerating info bars
// attached to a tab.
class InfoBarManager {
 public:
  // Observer class for infobar events.
  class Observer {
   public:
    virtual ~Observer();

    virtual void OnInfoBarAdded(InfoBar* infobar);
    virtual void OnInfoBarRemoved(InfoBar* infobar, bool animate);
    virtual void OnInfoBarReplaced(InfoBar* old_infobar,
                                   InfoBar* new_infobar);
    virtual void OnManagerShuttingDown(InfoBarManager* manager);
  };

  InfoBarManager();

  InfoBarManager(const InfoBarManager&) = delete;
  InfoBarManager& operator=(const InfoBarManager&) = delete;

  virtual ~InfoBarManager();

  // Must be called before destruction.
  // TODO(droger): Merge this method with the destructor once the virtual calls
  // for notifications are removed (see http://crbug.com/354380).
  void ShutDown();

  // Adds the specified |infobar|, which already owns a delegate.
  //
  // If infobars are disabled for this tab, |infobar| is deleted immediately.
  // If the tab already has an infobar whose delegate returns true for
  // InfoBarDelegate::EqualsDelegate(infobar->delegate()), depending on the
  // value of |replace_existing|, |infobar| is either deleted immediately
  // without being added, or is added as replacement for the matching infobar.
  //
  // Returns the infobar if it was successfully added.
  InfoBar* AddInfoBar(std::unique_ptr<InfoBar> infobar,
                      bool replace_existing = false);

  // Removes the specified |infobar|.  This in turn may close immediately or
  // animate closed; at the end the infobar will delete itself.
  //
  // If infobars are disabled for this tab, this will do nothing, on the
  // assumption that the matching AddInfoBar() call will have already deleted
  // the infobar (see above).
  void RemoveInfoBar(InfoBar* infobar);

  // Removes all the infobars.
  void RemoveAllInfoBars(bool animate);

  // Replaces one infobar with another, without any animation in between.  This
  // will result in |old_infobar| being synchronously deleted.
  //
  // If infobars are disabled for this tab, |new_infobar| is deleted immediately
  // without being added, and nothing else happens.
  //
  // Returns the new infobar if it was successfully added.
  //
  // NOTE: This does not perform any EqualsDelegate() checks like AddInfoBar().
  InfoBar* ReplaceInfoBar(InfoBar* old_infobar,
                          std::unique_ptr<InfoBar> new_infobar);

  // Returns managed infobars.
  const std::vector<raw_ptr<InfoBar, VectorExperimental>>& infobars() const {
    return infobars_;
  }

  // Must be called when a navigation happens.
  void OnNavigation(const InfoBarDelegate::NavigationDetails& details);

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  bool animations_enabled() const { return animations_enabled_; }

  // Returns the active entry ID.
  virtual int GetActiveEntryID() = 0;

  // Opens a URL according to the specified |disposition|.
  virtual void OpenURL(const GURL& url, WindowOpenDisposition disposition) = 0;

 protected:
  void set_animations_enabled(bool animations_enabled) {
    animations_enabled_ = animations_enabled;
  }

  bool ShouldShowInfoBar(const InfoBar* infobar) const;

 private:
  friend class ::TestInfoBar;

  // InfoBars associated with this InfoBarManager. We own these pointers.
  // However, this is not a vector of unique_ptr, because we don't delete the
  // infobars directly once they've been added to this; instead, when we're
  // done with an infobar, we instruct it to delete itself and then orphan it.
  // See RemoveInfoBarInternal().
  using InfoBars = std::vector<raw_ptr<InfoBar, VectorExperimental>>;

  void RemoveInfoBarInternal(InfoBar* infobar, bool animate);

  InfoBars infobars_;
  bool animations_enabled_ = true;
  const bool infobars_enabled_ = true;

  base::ObserverList<Observer, true>::Unchecked observer_list_;
};

}  // namespace infobars

#endif  // COMPONENTS_INFOBARS_CORE_INFOBAR_MANAGER_H_

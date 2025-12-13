// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CORE_INFOBAR_CONTAINER_H_
#define COMPONENTS_INFOBARS_CORE_INFOBAR_CONTAINER_H_

#include <stddef.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/infobars/core/features.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "third_party/skia/include/core/SkColor.h"

namespace infobars {

class InfoBar;

// InfoBarContainer is a cross-platform base class to handle the visibility-
// related aspects of InfoBars.  While InfoBarManager owns the InfoBars, the
// InfoBarContainer is responsible for telling particular InfoBars that they
// should be hidden or visible.
//
// Platforms need to subclass this to implement a few platform-specific
// functions, which are pure virtual here.
class InfoBarContainer : public InfoBarManager::Observer {
 public:
  class Delegate {
   public:
    // The delegate is notified each time the infobar container changes height,
    // as well as when it stops animating.
    virtual void InfoBarContainerStateChanged(bool is_animating) = 0;

   protected:
    virtual ~Delegate();
  };

  explicit InfoBarContainer(Delegate* delegate);

  InfoBarContainer(const InfoBarContainer&) = delete;
  InfoBarContainer& operator=(const InfoBarContainer&) = delete;

  ~InfoBarContainer() override;

  // Changes the InfoBarManager for which this container is showing infobars.
  // This will hide all current infobars, remove them from the container, add
  // the infobars from |infobar_manager|, and show them all.  |infobar_manager|
  // may be NULL.
  virtual void ChangeInfoBarManager(InfoBarManager* infobar_manager);

  const Delegate* delegate() const { return delegate_; }

  bool ShouldHideInFullscreen() const;

  // Called by |infobar| to request that it be removed from the container.  At
  // this point, |infobar| should already be hidden.
  void RemoveInfoBar(InfoBar* infobar);

  // Called when a contained infobar has animated or by some other means changed
  // its height, or when it stops animating.  The container is expected to do
  // anything necessary to respond, e.g. re-layout.
  void OnInfoBarStateChanged(bool is_animating);

 protected:
  typedef std::vector<raw_ptr<InfoBar, VectorExperimental>> InfoBars;

  // InfoBarManager::Observer:
  void OnInfoBarAdded(InfoBar* infobar) override;
  void OnInfoBarRemoved(InfoBar* infobar, bool animate) override;
  void OnInfoBarReplaced(InfoBar* old_infobar, InfoBar* new_infobar) override;
  void OnManagerWillBeDestroyed(InfoBarManager* manager) override;

  // Subclasses must call this during destruction, so that we can remove
  // infobars (which will call the pure virtual functions below) while the
  // subclass portion of |this| has not yet been destroyed.
  void RemoveAllInfoBarsForDestruction();

  // These must be implemented on each platform to e.g. adjust the visible
  // object hierarchy.  The first two functions should each be called exactly
  // once during an infobar's life (see comments on RemoveInfoBar() and
  // AddInfoBar()).
  virtual void PlatformSpecificAddInfoBar(InfoBar* infobar,
                                          size_t position) = 0;
  // TODO(miguelg): Remove this; it is only necessary for Android, and only
  // until the translate infobar is implemented as three different infobars like
  // GTK does.
  virtual void PlatformSpecificReplaceInfoBar(InfoBar* old_infobar,
                                              InfoBar* new_infobar) {}
  virtual void PlatformSpecificRemoveInfoBar(InfoBar* infobar) = 0;
  virtual void PlatformSpecificInfoBarStateChanged(bool is_animating) {}

  // Adds |infobar| to this container before the existing infobar at position
  // |position| and calls Show() on it.  |animate| is passed along to
  // infobar->Show().
  void AddInfoBar(InfoBar* infobar, size_t position, bool animate);

  const InfoBars& infobars() const { return infobars_; }
  InfoBars& infobars() { return infobars_; }

  // Returns the InfoBarManager that this object is observing.
  InfoBarManager* manager() { return scoped_observation_.GetSource(); }
  const InfoBarManager* manager() const {
    return scoped_observation_.GetSource();
  }

  // Non-owning pointer to the delegate that manages the view.
  raw_ptr<Delegate> delegate_;

  // Scoped observation for the InfoBarManager.
  base::ScopedObservation<InfoBarManager, InfoBarManager::Observer>
      scoped_observation_{this};

  // Normally false.  When true, OnInfoBarStateChanged() becomes a no-op.  We
  // use this to ensure that ChangeInfoBarManager() only executes the
  // functionality in OnInfoBarStateChanged() once, to minimize unnecessary
  // layout and painting.
  bool ignore_infobar_state_changed_;

 private:
  // The list of infobars currently shown in this container.
  InfoBars infobars_;
};

}  // namespace infobars

#endif  // COMPONENTS_INFOBARS_CORE_INFOBAR_CONTAINER_H_

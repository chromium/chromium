// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_VIEWS_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_VIEWS_DELEGATE_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/views/views_delegate.h"

class Profile;
class ScopedKeepAlive;
class ScopedProfileKeepAlive;

class ChromeViewsDelegate : public views::ViewsDelegate {
 public:
  ChromeViewsDelegate();

  ChromeViewsDelegate(const ChromeViewsDelegate&) = delete;
  ChromeViewsDelegate& operator=(const ChromeViewsDelegate&) = delete;

  ~ChromeViewsDelegate() override;

  // views::ViewsDelegate:
  void SaveWindowPlacement(const views::Widget* window,
                           const std::string& window_name,
                           const gfx::Rect& bounds,
                           ui::mojom::WindowShowState show_state) override;
  bool GetSavedWindowPlacement(
      const views::Widget* widget,
      const std::string& window_name,
      gfx::Rect* bounds,
      ui::mojom::WindowShowState* show_state) const override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ProcessMenuAcceleratorResult ProcessAcceleratorWhileMenuShowing(
      const ui::Accelerator& accelerator) override;
  bool ShouldCloseMenuIfMouseCaptureLost() const override;
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool ShouldWindowHaveRoundedCorners(
      const gfx::NativeWindow window) const override;
#endif

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<views::NonClientFrameView> CreateDefaultNonClientFrameView(
      views::Widget* widget) override;
#endif

#if BUILDFLAG(IS_WIN)
  HICON GetDefaultWindowIcon() const override;
  HICON GetSmallWindowIcon() const override;
  int GetAppbarAutohideEdges(HMONITOR monitor,
                             base::OnceClosure callback) override;
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  bool WindowManagerProvidesTitleBar(bool maximized) override;
#endif

#if BUILDFLAG(IS_LINUX)
  gfx::ImageSkia* GetDefaultWindowIcon() const override;
#endif

  void AddRef() override;
  void ReleaseRef() override;
  bool IsShuttingDown() const override;
  void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) override;
#if BUILDFLAG(IS_MAC)
  ui::ContextFactory* GetContextFactory() override;
#endif
  std::string GetApplicationName() override;

 private:
#if BUILDFLAG(IS_WIN)
  typedef std::map<HMONITOR, int> AppbarAutohideEdgeMap;

  // Callback on main thread with the edges. |returned_edges| is the value that
  // was returned from the call to GetAutohideEdges() that initiated the lookup.
  void OnGotAppbarAutohideEdges(base::OnceClosure callback,
                                HMONITOR monitor,
                                int returned_edges,
                                int edges);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Called from GetSavedWindowPlacement() on ChromeOS to adjust the bounds.
  void AdjustSavedWindowPlacementChromeOS(const views::Widget* widget,
                                          gfx::Rect* bounds) const;
#endif

  views::NativeWidget* CreateNativeWidget(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate);

  // |ChromeViewsDelegate| exposes a |RefCounted|-like interface, but //chrome
  // uses |ScopedKeepAlive|s to manage lifetime. We manage an internal counter
  // to do that translation.
  unsigned int ref_count_ = 0u;

  // Prevents BrowserProcess teardown while |ref_count_| is non-zero.
  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  // Prevents Profile* deletion while |ref_count_| is non-zero. See the
  // DestroyProfileOnBrowserClose flag.
  std::map<Profile*, std::unique_ptr<ScopedProfileKeepAlive>>
      profile_keep_alives_;

#if BUILDFLAG(IS_WIN)
  AppbarAutohideEdgeMap appbar_autohide_edge_map_;
  // If true we're in the process of notifying a callback from
  // GetAutohideEdges().start a new query.
  bool in_autohide_edges_callback_ = false;

  base::WeakPtrFactory<ChromeViewsDelegate> weak_factory_{this};
#endif
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_VIEWS_DELEGATE_H_

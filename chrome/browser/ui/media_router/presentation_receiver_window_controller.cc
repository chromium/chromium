// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/presentation_receiver_window_controller.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/ui/media_router/presentation_receiver_window.h"
#include "components/media_router/browser/presentation/receiver_presentation_service_delegate_impl.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/presentation_receiver_flags.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"

using content::WebContents;

namespace {

WebContents::CreateParams CreateWebContentsParams(Profile* profile) {
  WebContents::CreateParams params(profile);
  params.starting_sandbox_flags = content::kPresentationReceiverSandboxFlags;
  return params;
}

}  // namespace

// static
std::unique_ptr<PresentationReceiverWindowController>
PresentationReceiverWindowController::CreateFromOriginalProfile(
    Profile* profile,
    const gfx::Rect& bounds,
    base::OnceClosure termination_callback,
    TitleChangeCallback title_change_callback) {
  DCHECK(profile);
  DCHECK(!profile->IsOffTheRecord());
  DCHECK(termination_callback);
  return base::WrapUnique(new PresentationReceiverWindowController(
      profile, bounds, std::move(termination_callback),
      std::move(title_change_callback)));
}

PresentationReceiverWindowController::~PresentationReceiverWindowController() {
  DCHECK(!web_contents_);
  DCHECK(!window_);

  if (otr_profile_) {
    otr_profile_observation_.Reset();
    ProfileDestroyer::DestroyOTRProfileWhenAppropriate(otr_profile_);
  }
}

void PresentationReceiverWindowController::Start(
    const std::string& presentation_id,
    const GURL& start_url) {
  DCHECK(window_);
  DCHECK(web_contents_);

  media_router::ReceiverPresentationServiceDelegateImpl::CreateForWebContents(
      web_contents_.get(), presentation_id);

  content::NavigationController::LoadURLParams load_params(start_url);
  load_params.should_replace_current_entry = true;
  load_params.should_clear_history_list = true;
  web_contents_->GetController().LoadURLWithParams(load_params);

  window_->ShowInactiveFullscreen();
}

void PresentationReceiverWindowController::Terminate() {
  if (web_contents_) {
    web_contents_->ClosePage();
  } else if (window_) {
    window_->Close();
  } else if (termination_callback_) {
    std::move(termination_callback_).Run();
  }
}

void PresentationReceiverWindowController::ExitFullscreen() {
  window_->ExitFullscreen();
}

void PresentationReceiverWindowController::CloseWindowForTest() {
  window_->Close();
}

bool PresentationReceiverWindowController::IsWindowActiveForTest() const {
  return window_->IsWindowActive();
}

bool PresentationReceiverWindowController::IsWindowFullscreenForTest() const {
  return window_->IsWindowFullscreen();
}

gfx::Rect PresentationReceiverWindowController::GetWindowBoundsForTest() const {
  return window_->GetWindowBounds();
}

WebContents* PresentationReceiverWindowController::web_contents() const {
  return web_contents_.get();
}

PresentationReceiverWindowController::PresentationReceiverWindowController(
    Profile* profile,
    const gfx::Rect& bounds,
    base::OnceClosure termination_callback,
    TitleChangeCallback title_change_callback)
    : otr_profile_(profile->GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUniqueForMediaRouter(),
          /*create_if_needed=*/true)),
      web_contents_(WebContents::Create(CreateWebContentsParams(otr_profile_))),
      window_(PresentationReceiverWindow::Create(this, bounds)),
      termination_callback_(std::move(termination_callback)),
      title_change_callback_(std::move(title_change_callback)) {
  DCHECK(otr_profile_);
  DCHECK(otr_profile_->IsOffTheRecord());
  otr_profile_observation_.Observe(otr_profile_.get());
  content::WebContentsObserver::Observe(web_contents_.get());
  web_contents_->SetDelegate(this);
}

void PresentationReceiverWindowController::WindowClosed() {
  window_ = nullptr;
  Terminate();
}

void PresentationReceiverWindowController::OnProfileWillBeDestroyed(
    Profile* profile) {
  DCHECK(profile == otr_profile_);
  web_contents_.reset();
  otr_profile_observation_.Reset();
  otr_profile_ = nullptr;
  Terminate();
}

void PresentationReceiverWindowController::DidStartNavigation(
    content::NavigationHandle* handle) {
  if (!navigation_policy_.AllowNavigation(handle))
    Terminate();
}

void PresentationReceiverWindowController::TitleWasSet(
    content::NavigationEntry* entry) {
  window_->UpdateWindowTitle();
  if (entry)
    title_change_callback_.Run(base::UTF16ToUTF8(entry->GetTitle()));
}

void PresentationReceiverWindowController::NavigationStateChanged(
    WebContents* source,
    content::InvalidateTypes changed_flags) {
  window_->UpdateLocationBar();
}

void PresentationReceiverWindowController::CloseContents(
    content::WebContents* source) {
  web_contents_.reset();
  Terminate();
}

bool PresentationReceiverWindowController::ShouldSuppressDialogs(
    content::WebContents* source) {
  DCHECK_EQ(web_contents_.get(), source);
  // Suppress all because there is no possible direct user interaction with
  // dialogs.
  // TODO(crbug.com/40526231): This does not suppress window.print().
  return true;
}

bool PresentationReceiverWindowController::ShouldFocusLocationBarByDefault(
    content::WebContents* source) {
  DCHECK_EQ(web_contents_.get(), source);
  // Indicate the location bar should be focused instead of the page, even
  // though there is no location bar in fullscreen (the presentation's default
  // state).  This will prevent the page from automatically receiving input
  // focus, which is usually not intended.
  return true;
}

bool PresentationReceiverWindowController::ShouldFocusPageAfterCrash(
    content::WebContents* source) {
  // Never focus the page after a crash.
  return false;
}

void PresentationReceiverWindowController::CanDownload(
    const GURL& url,
    const std::string& request_method,
    base::OnceCallback<void(bool)> callback) {
  // Local presentation pages are not allowed to download files.
  std::move(callback).Run(false);
}

bool PresentationReceiverWindowController::IsWebContentsCreationOverridden(
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  // Disallow creating separate WebContentses.  The WebContents implementation
  // uses this to spawn new windows/tabs, which is also not allowed for
  // local presentations.
  return true;
}

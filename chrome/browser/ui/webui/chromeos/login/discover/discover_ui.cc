// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/discover/discover_ui.h"

#include "base/logging.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_window_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

namespace {

// Data to store a reference to DiscoverUI into WebContents.
class DiscoverUIUserDataPointer : public base::SupportsUserData::Data {
 public:
  static DiscoverUI* Get(const content::WebContents* web_contents) {
    DiscoverUIUserDataPointer* data = static_cast<DiscoverUIUserDataPointer*>(
        web_contents->GetUserData(kKey));
    if (data)
      return data->ptr_;

    return nullptr;
  }

  static void Attach(DiscoverUI* discover_ui,
                     content::WebContents* web_contents) {
    DCHECK(!Get(web_contents));

    web_contents->SetUserData(
        kKey, std::make_unique<DiscoverUIUserDataPointer>(discover_ui));
  }

  static void Detach(const DiscoverUI* discover_ui,
                     content::WebContents* web_contents) {
#if DCHECK_IS_ON()
    if (Get(web_contents))
      DCHECK_EQ(Get(web_contents), discover_ui);
#else
    ANALYZER_ALLOW_UNUSED(discover_ui);
#endif

    web_contents->RemoveUserData(kKey);
  }

  explicit DiscoverUIUserDataPointer(DiscoverUI* discover_ui)
      : ptr_(discover_ui) {}

  ~DiscoverUIUserDataPointer() override = default;

 private:
  DiscoverUI* ptr_ = nullptr;

  static const char kKey[];

  DISALLOW_COPY_AND_ASSIGN(DiscoverUIUserDataPointer);
};

const char DiscoverUIUserDataPointer::kKey[] = "DiscoverUIKey";
}  // anonymous namespace

DiscoverUI::DiscoverUI() = default;

DiscoverUI::~DiscoverUI() {
  if (web_ui_)
    DiscoverUIUserDataPointer::Detach(this, web_ui_->GetWebContents());
}

DiscoverUI* DiscoverUI::GetDiscoverUI(
    const content::WebContents* web_contents) {
  return DiscoverUIUserDataPointer::Get(web_contents);
}

void DiscoverUI::RegisterMessages(content::WebUI* web_ui) {
  DCHECK(!web_ui_);
  web_ui_ = web_ui;

  DiscoverUIUserDataPointer::Attach(this, web_ui->GetWebContents());

  std::vector<std::unique_ptr<DiscoverHandler>> handlers =
      DiscoverManager::Get()->CreateWebUIHandlers(&js_calls_container_);
  for (auto& handler : handlers) {
    handlers_.push_back(handler.get());
    web_ui->AddMessageHandler(std::move(handler));
  }
  initialized_ = true;
}

void DiscoverUI::GetAdditionalParameters(base::DictionaryValue* dict) {
  CHECK(initialized_);
  for (DiscoverHandler* handler : handlers_) {
    handler->GetLocalizedStrings(dict);
  }
}

void DiscoverUI::Initialize() {
  for (Observer& observer : observers_)
    observer.OnInitialized();
  js_calls_container_.ExecuteDeferredJSCalls(web_ui_);
}

void DiscoverUI::Show() {}

void DiscoverUI::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DiscoverUI::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace chromeos

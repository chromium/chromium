// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/image_editor/editor_untrusted_source.h"

#include <string>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/url_util.h"

EditorUntrustedSource::EditorUntrustedSource(Profile* profile) {}

EditorUntrustedSource::~EditorUntrustedSource() = default;

std::string EditorUntrustedSource::GetContentSecurityPolicy(
    network::mojom::CSPDirectiveName directive) {
  // TODO(kmilka): Determine correct CSP for the page.
  switch (directive) {
    case network::mojom::CSPDirectiveName::ScriptSrc:
      return std::string();
    case network::mojom::CSPDirectiveName::ChildSrc:
      return std::string();
    case network::mojom::CSPDirectiveName::DefaultSrc:
      return std::string();
    case network::mojom::CSPDirectiveName::FrameAncestors:
      return std::string();
    case network::mojom::CSPDirectiveName::RequireTrustedTypesFor:
      return std::string();
    case network::mojom::CSPDirectiveName::TrustedTypes:
      return std::string();
    default:
      return content::URLDataSource::GetContentSecurityPolicy(directive);
  }
}

std::string EditorUntrustedSource::GetSource() {
  return chrome::kChromeUIUntrustedImageEditorURL;
}

void EditorUntrustedSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  const std::string path = content::URLDataSource::URLToRequestPath(url);
  if (path == "placeholder") {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    base::RefCountedMemory* bytes =
        bundle.LoadDataResourceBytes(IDR_IMAGE_EDITOR_UNTRUSTED_HTML);
    base::StringPiece string_piece(
        reinterpret_cast<const char*>(bytes->front()), bytes->size());

    ui::TemplateReplacements replacements;
    replacements["textdirection"] = base::i18n::IsRTL() ? "rtl" : "ltr";
    const std::string& app_locale = g_browser_process->GetApplicationLocale();
    replacements["language"] = l10n_util::GetLanguage(app_locale);
    std::string html = ui::ReplaceTemplateExpressions(
        string_piece, replacements,
        /* skip_unexpected_placeholder_check= */ true);

    std::move(callback).Run(base::RefCountedString::TakeString(&html));
  }
}

std::string EditorUntrustedSource::GetMimeType(const std::string& path) {
  const std::string stripped_path = path.substr(0, path.find("?"));
  if (base::EndsWith(stripped_path, ".js",
                     base::CompareCase::INSENSITIVE_ASCII)) {
    return "application/javascript";
  }
  return "text/html";
}

bool EditorUntrustedSource::AllowCaching() {
  return false;
}

bool EditorUntrustedSource::ShouldReplaceExistingSource() {
  return false;
}

bool EditorUntrustedSource::ShouldServeMimeTypeAsContentTypeHeader() {
  return true;
}

bool EditorUntrustedSource::ShouldServiceRequest(
    const GURL& url,
    content::BrowserContext* browser_context,
    int render_process_id) {
  return url.DeprecatedGetOriginAsURL() == GetSource();
}

bool EditorUntrustedSource::ShouldDenyXFrameOptions() {
  return false;
}

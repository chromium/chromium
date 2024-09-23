// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_settings_agent_delegate.h"

#include "build/chromeos_buildflags.h"
#include "pdf/buildflags.h"

// TODO(b/197163596): Remove File Manager constants
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/file_manager/url_constants.h"
#endif
#include "base/containers/contains.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_local_frame.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/renderer_extension_registry.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/common/pdf_util.h"
#include "third_party/blink/public/web/web_frame.h"
#include "url/origin.h"
#endif

ChromeContentSettingsAgentDelegate::ChromeContentSettingsAgentDelegate(
    content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      RenderFrameObserverTracker<ChromeContentSettingsAgentDelegate>(
          render_frame),
      render_frame_(render_frame) {
  content::RenderFrame* main_frame = render_frame->GetMainRenderFrame();
  // TODO(nasko): The main frame is not guaranteed to be in the same process
  // with this frame with --site-per-process. This code needs to be updated
  // to handle this case. See https://crbug.com/496670.
  if (main_frame && main_frame != render_frame) {
    auto* parent = ChromeContentSettingsAgentDelegate::Get(main_frame);
    temporarily_allowed_plugins_ = parent->temporarily_allowed_plugins_;
  }
}

ChromeContentSettingsAgentDelegate::~ChromeContentSettingsAgentDelegate() =
    default;

#if BUILDFLAG(ENABLE_EXTENSIONS)
void ChromeContentSettingsAgentDelegate::SetExtensionDispatcher(
    extensions::Dispatcher* extension_dispatcher) {
  DCHECK(!extension_dispatcher_)
      << "SetExtensionDispatcher() should only be called once.";
  extension_dispatcher_ = extension_dispatcher;
}
#endif

bool ChromeContentSettingsAgentDelegate::IsPluginTemporarilyAllowed(
    const std::string& identifier) {
  // If the empty string is in here, it means all plugins are allowed.
  // TODO(bauerb): Remove this once we only pass in explicit identifiers.
  return base::Contains(temporarily_allowed_plugins_, identifier) ||
         base::Contains(temporarily_allowed_plugins_, std::string());
}

void ChromeContentSettingsAgentDelegate::AllowPluginTemporarily(
    const std::string& identifier) {
  temporarily_allowed_plugins_.insert(identifier);
}

bool ChromeContentSettingsAgentDelegate::IsFrameAllowlistedForStorageAccess(
    blink::WebFrame* frame) const {
#if BUILDFLAG(ENABLE_PDF)
  // Allow the Chrome PDF Viewer's extension frame to access storage. This is
  // needed when a data: URL navigates to or embeds a PDF. Normally, data: URLs
  // are opaque and shouldn't be able to access storage. However, the Chrome PDF
  // viewer is an internal use case and does not need to adhere to the web spec.

  // The origin should match the PDF extension's origin. A PDF extension frame
  // should always have a parent (the PDF embedder frame).
  if (IsPdfExtensionOrigin(url::Origin(frame->GetSecurityOrigin())) &&
      frame->Parent()) {
    return true;
  }
#endif
  return false;
}

bool ChromeContentSettingsAgentDelegate::IsSchemeAllowlisted(
    const std::string& scheme) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return scheme == extensions::kExtensionScheme;
#else
  return false;
#endif
}

bool ChromeContentSettingsAgentDelegate::AllowReadFromClipboard() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ScriptContext* current_context =
      extension_dispatcher_->script_context_set().GetCurrent();
  if (current_context &&
      current_context->HasAPIPermission(
          extensions::mojom::APIPermissionID::kClipboardRead)) {
    return true;
  }

  if (IsAllowListedSystemWebApp()) {
    return true;
  }
#endif
  return false;
}

bool ChromeContentSettingsAgentDelegate::AllowWriteToClipboard() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // All blessed extension pages could historically write to the clipboard, so
  // preserve that for compatibility.
  extensions::ScriptContext* current_context =
      extension_dispatcher_->script_context_set().GetCurrent();
  if (current_context) {
    if (current_context->effective_context_type() ==
            extensions::mojom::ContextType::kPrivilegedExtension &&
        !current_context->IsForServiceWorker()) {
      return true;
    }
    if (current_context->HasAPIPermission(
            extensions::mojom::APIPermissionID::kClipboardWrite)) {
      return true;
    }
  }
#endif
  return false;
}

std::optional<bool> ChromeContentSettingsAgentDelegate::AllowMutationEvents() {
  if (IsPlatformApp())
    return false;
  return std::nullopt;
}

void ChromeContentSettingsAgentDelegate::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  if (render_frame()->GetWebFrame()->Parent())
    return;

  temporarily_allowed_plugins_.clear();
}

void ChromeContentSettingsAgentDelegate::OnDestruct() {}

bool ChromeContentSettingsAgentDelegate::IsPlatformApp() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  blink::WebLocalFrame* frame = render_frame_->GetWebFrame();
  blink::WebSecurityOrigin origin = frame->GetDocument().GetSecurityOrigin();
  const extensions::Extension* extension = GetExtension(origin);
  return extension && extension->is_platform_app();
#else
  return false;
#endif
}

bool ChromeContentSettingsAgentDelegate::IsAllowListedSystemWebApp() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  blink::WebLocalFrame* frame = render_frame_->GetWebFrame();
  blink::WebSecurityOrigin origin = frame->GetDocument().GetSecurityOrigin();
  // TODO(crbug.com/1233395): Migrate Files SWA to Clipboard API and remove this
  // allow-list.
  if (origin.Protocol().Ascii() == ::content::kChromeUIScheme &&
      origin.Host().Utf8() == ::ash::file_manager::kChromeUIFileManagerHost) {
    return true;
  }
#endif
  return false;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
const extensions::Extension* ChromeContentSettingsAgentDelegate::GetExtension(
    const blink::WebSecurityOrigin& origin) const {
  if (origin.Protocol().Ascii() != extensions::kExtensionScheme)
    return nullptr;

  const std::string extension_id = origin.Host().Utf8().data();
  if (!extension_dispatcher_->IsExtensionActive(extension_id))
    return nullptr;

  return extensions::RendererExtensionRegistry::Get()->GetByID(extension_id);
}
#endif

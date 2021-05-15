// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/telemetry_extension_ui/telemetry_extension_untrusted_ui.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromeos/components/telemetry_extension_ui/url_constants.h"
#include "chromeos/grit/chromeos_telemetry_extension_resources.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/url_constants.h"
#include "net/base/mime_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

namespace {

constexpr char kDefaultMime[] = "text/html";

base::FilePath GetTelemetryDirectory() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool has_switch =
      command_line->HasSwitch(chromeos::switches::kTelemetryExtensionDirectory);

  if (!has_switch) {
    LOG(WARNING) << "Switch value '--telemetry-extension-dir' is not "
                    "specified, some resources might not be loaded";
    return base::FilePath();
  }

  return base::FilePath(command_line->GetSwitchValueASCII(
      chromeos::switches::kTelemetryExtensionDirectory));
}

void ReadFile(const base::FilePath& path,
              content::URLDataSource::GotDataCallback callback) {
  std::string content;
  if (!base::ReadFileToString(path, &content)) {
    PLOG(ERROR) << "Failed to read content from file: " << path;
    std::move(callback).Run(nullptr);
    return;
  }

  scoped_refptr<base::RefCountedString> response =
      base::RefCountedString::TakeString(&content);
  std::move(callback).Run(response.get());
}

// A data source that helps with loading resources needed by
// chrome-untrusted://telemetry-extension/ WebUI pages.
// There are two types of resources:
// 1. GRIT resourse if the resource path exist in |path_to_idr_map_|; otherwise,
// 2. Resource from the directory specified by
// |chromeos::switches::kTelemetryExtensionDirectory| command line switch.
class TelemetryExtensionUntrustedSource : public content::URLDataSource {
 public:
  explicit TelemetryExtensionUntrustedSource(const std::string& source);

  TelemetryExtensionUntrustedSource(const TelemetryExtensionUntrustedSource&) =
      delete;
  TelemetryExtensionUntrustedSource& operator=(
      const TelemetryExtensionUntrustedSource&) = delete;
  ~TelemetryExtensionUntrustedSource() override;

  void AddResourcePath(base::StringPiece path, int resource_id);
  void OverrideContentSecurityPolicy(network::mojom::CSPDirectiveName directive,
                                     const std::string& value);

  // content::URLDataSource overrides:
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const std::string& path) override;
  std::string GetContentSecurityPolicy(
      network::mojom::CSPDirectiveName directive) override;

 private:
  absl::optional<int> PathToIdr(const std::string& path);

  const base::FilePath root_directory_;
  const std::string source_;
  std::map<std::string, int> path_to_idr_map_;
  base::flat_map<network::mojom::CSPDirectiveName, std::string>
      csp_overrides_map_;
};

TelemetryExtensionUntrustedSource::TelemetryExtensionUntrustedSource(
    const std::string& source)
    : root_directory_(GetTelemetryDirectory()), source_(source) {}

TelemetryExtensionUntrustedSource::~TelemetryExtensionUntrustedSource() =
    default;

void TelemetryExtensionUntrustedSource::AddResourcePath(base::StringPiece path,
                                                        int resource_id) {
  path_to_idr_map_[std::string(path)] = resource_id;
}

void TelemetryExtensionUntrustedSource::OverrideContentSecurityPolicy(
    network::mojom::CSPDirectiveName directive,
    const std::string& value) {
  csp_overrides_map_.insert_or_assign(directive, value);
}

std::string TelemetryExtensionUntrustedSource::GetSource() {
  return source_;
}

void TelemetryExtensionUntrustedSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  std::string path = content::URLDataSource::URLToRequestPath(url);
  // Remove the query string for named resource lookups.
  path = path.substr(0, path.find_first_of('?'));

  absl::optional<int> resource_id = PathToIdr(path);
  if (resource_id.has_value()) {
    scoped_refptr<base::RefCountedMemory> response(
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
            resource_id.value()));
    std::move(callback).Run(response.get());
    return;
  }

  if (root_directory_.empty()) {
    LOG(WARNING) << "Cannot load resources from disk, switch value "
                    "'--telemetry-extension-dir' is not specified";
    std::move(callback).Run(nullptr);
    return;
  }

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ReadFile, root_directory_.Append(path),
                     std::move(callback)));
}

std::string TelemetryExtensionUntrustedSource::GetMimeType(
    const std::string& path) {
  const std::string ext = base::FilePath(path).Extension();
  if (ext.empty()) {
    return kDefaultMime;
  }

  std::string mime_type;
  net::GetWellKnownMimeTypeFromExtension(ext.substr(1), &mime_type);
  return mime_type;
}

std::string TelemetryExtensionUntrustedSource::GetContentSecurityPolicy(
    network::mojom::CSPDirectiveName directive) {
  const auto& it = csp_overrides_map_.find(directive);
  if (it == csp_overrides_map_.end()) {
    return URLDataSource::GetContentSecurityPolicy(directive);
  }
  return it->second;
}

absl::optional<int> TelemetryExtensionUntrustedSource::PathToIdr(
    const std::string& path) {
  const auto& it = path_to_idr_map_.find(path);
  if (it == path_to_idr_map_.end()) {
    return absl::nullopt;
  }
  return absl::optional<int>(it->second);
}

}  // namespace

TelemetryExtensionUntrustedUIConfig::TelemetryExtensionUntrustedUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  chromeos::kChromeUITelemetryExtensionHost) {}

TelemetryExtensionUntrustedUIConfig::~TelemetryExtensionUntrustedUIConfig() =
    default;

std::unique_ptr<content::WebUIController>
TelemetryExtensionUntrustedUIConfig::CreateWebUIController(
    content::WebUI* web_ui) {
  return std::make_unique<TelemetryExtensionUntrustedUI>(web_ui);
}

TelemetryExtensionUntrustedUI::TelemetryExtensionUntrustedUI(
    content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  auto untrusted_source = std::make_unique<TelemetryExtensionUntrustedSource>(
      chromeos::kChromeUIUntrustedTelemetryExtensionURL);

  untrusted_source->AddResourcePath("dpsl.js", IDR_TELEMETRY_EXTENSION_DPSL_JS);

  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameAncestors,
      std::string("frame-ancestors ") +
          chromeos::kChromeUITelemetryExtensionURL + ";");
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src 'self';");
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types telemetry-extension-static;");

  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::URLDataSource::Add(browser_context, std::move(untrusted_source));
}

TelemetryExtensionUntrustedUI::~TelemetryExtensionUntrustedUI() = default;

}  // namespace chromeos

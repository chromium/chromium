// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/about/about_ui.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/about_ui/credit_utils.h"
#include "components/grit/components_resources.h"
#include "components/strings/grit/components_locale_settings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/url_constants.h"
#include "net/base/filename_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/theme_source.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include <map>

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/borealis/borealis_credits.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/customization/customization_document.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/component_updater/ash/component_manager_ash.h"
#include "components/language/core/common/locale_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/zlib/google/compression_utils.h"
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/lacros_url_handling.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/common/webui_url_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using content::BrowserThread;

namespace {

constexpr char kCreditsJsPath[] = "credits.js";
constexpr char kCreditsCssPath[] = "credits.css";
constexpr char kStatsJsPath[] = "stats.js";
constexpr char kStringsJsPath[] = "strings.js";

#if BUILDFLAG(IS_CHROMEOS_ASH)

constexpr char kTerminaCreditsPath[] = "about_os_credits.html";

// Loads bundled terms of service contents (Eula, OEM Eula, Play Store Terms).
// The online version of terms is fetched in OOBE screen javascript. This is
// intentional because chrome://terms runs in a privileged webui context and
// should never load from untrusted places.
class ChromeOSTermsHandler
    : public base::RefCountedThreadSafe<ChromeOSTermsHandler> {
 public:
  ChromeOSTermsHandler(const ChromeOSTermsHandler&) = delete;
  ChromeOSTermsHandler& operator=(const ChromeOSTermsHandler&) = delete;

  static void Start(const std::string& path,
                    content::URLDataSource::GotDataCallback callback) {
    scoped_refptr<ChromeOSTermsHandler> handler(
        new ChromeOSTermsHandler(path, std::move(callback)));
    handler->StartOnUIThread();
  }

 private:
  friend class base::RefCountedThreadSafe<ChromeOSTermsHandler>;

  ChromeOSTermsHandler(const std::string& path,
                       content::URLDataSource::GotDataCallback callback)
      : path_(path),
        callback_(std::move(callback)),
        // Previously we were using "initial locale" http://crbug.com/145142
        locale_(g_browser_process->GetApplicationLocale()) {}

  virtual ~ChromeOSTermsHandler() {}

  void StartOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (path_ == chrome::kOemEulaURLPath) {
      // Load local OEM EULA from the disk.
      base::ThreadPool::PostTaskAndReply(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
          base::BindOnce(&ChromeOSTermsHandler::LoadOemEulaFileAsync, this),
          base::BindOnce(&ChromeOSTermsHandler::ResponseOnUIThread, this));
    } else if (path_ == chrome::kArcTermsURLPath) {
      LOG(WARNING) << "Could not load offline Play Store ToS.";
    } else if (path_ == chrome::kArcPrivacyPolicyURLPath) {
      LOG(WARNING) << "Could not load offline Play Store privacy policy.";
    } else {
      NOTREACHED_IN_MIGRATION();
      ResponseOnUIThread();
    }
  }

  void LoadOemEulaFileAsync() {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    const ash::StartupCustomizationDocument* customization =
        ash::StartupCustomizationDocument::GetInstance();
    if (!customization->IsReady())
      return;

    base::FilePath oem_eula_file_path;
    if (net::FileURLToFilePath(GURL(customization->GetEULAPage(locale_)),
                               &oem_eula_file_path)) {
      if (!base::ReadFileToString(oem_eula_file_path, &contents_)) {
        contents_.clear();
      }
    }
  }

  void ResponseOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // If we fail to load Chrome OS EULA from disk, load it from resources.
    // Do nothing if OEM EULA or Play Store ToS load failed.
    if (contents_.empty() && path_.empty()) {
      contents_ =
          ui::ResourceBundle::GetSharedInstance().LoadLocalizedResourceString(
              IDS_TERMS_HTML);
    }
    std::move(callback_).Run(
        base::MakeRefCounted<base::RefCountedString>(std::move(contents_)));
  }

  // Path in the URL.
  const std::string path_;

  // Callback to run with the response.
  content::URLDataSource::GotDataCallback callback_;

  // Locale of the EULA.
  const std::string locale_;

  // EULA contents that was loaded from file.
  std::string contents_;
};

class ChromeOSCreditsHandler
    : public base::RefCountedThreadSafe<ChromeOSCreditsHandler> {
 public:
  ChromeOSCreditsHandler(const ChromeOSCreditsHandler&) = delete;
  ChromeOSCreditsHandler& operator=(const ChromeOSCreditsHandler&) = delete;

  // |prefix| allows tests to specify different location for the credits files.
  static void Start(const std::string& path,
                    content::URLDataSource::GotDataCallback callback,
                    const base::FilePath& prefix) {
    scoped_refptr<ChromeOSCreditsHandler> handler(
        new ChromeOSCreditsHandler(path, std::move(callback), prefix));
    handler->StartOnUIThread();
  }

 private:
  friend class base::RefCountedThreadSafe<ChromeOSCreditsHandler>;

  ChromeOSCreditsHandler(const std::string& path,
                         content::URLDataSource::GotDataCallback callback,
                         const base::FilePath& prefix)
      : path_(path), callback_(std::move(callback)), prefix_(prefix) {}

  virtual ~ChromeOSCreditsHandler() {}

  void StartOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // Load local Chrome OS credits from the disk.
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&ChromeOSCreditsHandler::LoadCreditsFileAsync, this),
        base::BindOnce(&ChromeOSCreditsHandler::ResponseOnUIThread, this));
  }

  // LoadCreditsFileAsync first attempts to load the uncompressed credits file.
  // Then, if that's not present, it attempts to load and decompress the
  // compressed credits file.
  // If both fails, fall back to default contents as handled in
  // ResponseOnUIThread.
  void LoadCreditsFileAsync() {
    if (prefix_.empty()) {
      prefix_ = base::FilePath(chrome::kChromeOSCreditsPath).DirName();
    }
    base::FilePath credits =
        prefix_.Append(base::FilePath(chrome::kChromeOSCreditsPath).BaseName());
    if (base::ReadFileToString(credits, &contents_)) {
      // Decompressed present; return.
      return;
    }

    // Decompressed not present; load compressed.
    base::FilePath compressed_credits = prefix_.Append(
        base::FilePath(chrome::kChromeOSCreditsCompressedPath).BaseName());
    std::string compressed;
    if (!base::ReadFileToString(compressed_credits, &compressed)) {
      // File with credits not found, ResponseOnUIThread will load credits
      // from resources if contents_ is empty.
      contents_.clear();
      return;
    }

    // Decompress.
    if (!compression::GzipUncompress(compressed, &contents_)) {
      LOG(DFATAL) << "Decompressing os credits failed";
      contents_.clear();
      return;
    }
  }

  void ResponseOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // If we fail to load Chrome OS credits from disk, load it from resources.
    if (contents_.empty()) {
      contents_ =
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
              IDR_OS_CREDITS_HTML);
    }
    std::move(callback_).Run(
        base::MakeRefCounted<base::RefCountedString>(std::move(contents_)));
  }

  // Path in the URL.
  const std::string path_;

  // Callback to run with the response.
  content::URLDataSource::GotDataCallback callback_;

  // Chrome OS credits contents that was loaded from file.
  std::string contents_;

  // Directory containing files to read.
  base::FilePath prefix_;
};

void OnBorealisCreditsLoaded(content::URLDataSource::GotDataCallback callback,
                             std::string credits_html) {
  if (credits_html.empty()) {
    credits_html = l10n_util::GetStringUTF8(IDS_BOREALIS_CREDITS_PLACEHOLDER);
  }
  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::move(credits_html)));
}

void HandleBorealisCredits(Profile* profile,
                           content::URLDataSource::GotDataCallback callback) {
  borealis::LoadBorealisCredits(
      profile, base::BindOnce(&OnBorealisCreditsLoaded, std::move(callback)));
}

class CrostiniCreditsHandler
    : public base::RefCountedThreadSafe<CrostiniCreditsHandler> {
 public:
  CrostiniCreditsHandler(const CrostiniCreditsHandler&) = delete;
  CrostiniCreditsHandler& operator=(const CrostiniCreditsHandler&) = delete;

  static void Start(Profile* profile,
                    const std::string& path,
                    content::URLDataSource::GotDataCallback callback) {
    scoped_refptr<CrostiniCreditsHandler> handler(
        new CrostiniCreditsHandler(profile, path, std::move(callback)));
    handler->StartOnUIThread();
  }

 private:
  friend class base::RefCountedThreadSafe<CrostiniCreditsHandler>;

  CrostiniCreditsHandler(Profile* profile,
                         const std::string& path,
                         content::URLDataSource::GotDataCallback callback)
      : path_(path), callback_(std::move(callback)), profile_(profile) {}

  virtual ~CrostiniCreditsHandler() {}

  void StartOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (crostini::CrostiniFeatures::Get()->IsAllowedNow(profile_)) {
      crostini::CrostiniManager::GetForProfile(profile_)->GetInstallLocation(
          base::BindOnce(&CrostiniCreditsHandler::LoadCredits, this));
    } else {
      RespondWithPlaceholder();
    }
  }

  void LoadCredits(base::FilePath path) {
    if (path.empty()) {
      RespondWithPlaceholder();
      return;
    }

    // Load crostini credits from the disk.
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&CrostiniCreditsHandler::LoadCrostiniCreditsFileAsync,
                       this, path.Append(kTerminaCreditsPath)),
        base::BindOnce(&CrostiniCreditsHandler::RespondOnUIThread, this));
  }

  void LoadCrostiniCreditsFileAsync(base::FilePath credits_file_path) {
    if (!base::ReadFileToString(credits_file_path, &contents_)) {
      // File with credits not found, RespondOnUIThread will load a placeholder
      // if contents_ is empty.
      contents_.clear();
    }
  }

  void RespondWithPlaceholder() {
    contents_.clear();
    RespondOnUIThread();
  }

  void RespondOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // If we fail to load Linux credits from disk, use the placeholder.
    if (contents_.empty()) {
      contents_ = l10n_util::GetStringUTF8(IDS_CROSTINI_CREDITS_PLACEHOLDER);
    }
    std::move(callback_).Run(
        base::MakeRefCounted<base::RefCountedString>(std::move(contents_)));
  }

  // Path in the URL.
  const std::string path_;

  // Callback to run with the response.
  content::URLDataSource::GotDataCallback callback_;

  // Linux credits contents that was loaded from file.
  std::string contents_;

  raw_ptr<Profile> profile_;
};
#endif

}  // namespace

// Individual about handlers ---------------------------------------------------

namespace about_ui {

void AppendHeader(std::string* output, const std::string& unescaped_title) {
  output->append("<!DOCTYPE HTML>\n<html>\n<head>\n");
  output->append("<meta charset='utf-8'>\n");
  output->append("<meta name='color-scheme' content='light dark'>\n");
  if (!unescaped_title.empty()) {
    output->append("<title>");
    output->append(base::EscapeForHTML(unescaped_title));
    output->append("</title>\n");
  }
}

#if BUILDFLAG(IS_CHROMEOS)
// This function returns true if Lacros is the primary browser - or if the
// calling browser is Lacros.
bool isLacrosPrimaryOrCurrentBrowser() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return crosapi::browser_util::IsLacrosEnabled();
#else
  return true;
#endif
}

void AppendBody(std::string* output) {
  if (isLacrosPrimaryOrCurrentBrowser()) {
    output->append(
        "<link rel='stylesheet' href='chrome://resources/css/os_header.css'>\n"

        "</head>\n<body>\n"

        "<div class='os-link-container-container' id='os-link-container'>\n"
        "<div class='os-link-container'>\n"
        "<span class='os-link-icon'></span>\n"
        "<span aria-hidden='true' id='os-link-desc'>" +
        l10n_util::GetStringUTF8(IDS_ABOUT_OS_TEXT1_LABEL) +
        "</span>\n"
        "<a href='#' id='os-link-href' aria-describedby='os-link-desc'>" +
        l10n_util::GetStringUTF8(IDS_ABOUT_OS_LINK) +
        "</a>\n<span aria-hidden='true'>" +
        l10n_util::GetStringUTF8(IDS_ABOUT_OS_TEXT2_LABEL) +
        "</span>\n</div>\n</div>\n");
  } else {
    output->append("</head>\n<body>\n");
  }
}

void AppendFooter(std::string* output) {
  if (isLacrosPrimaryOrCurrentBrowser()) {
    output->append(
        "<script type='module' src='chrome://resources/js/os_about.js'>"
        "</script>\n");
  }

  output->append("</body>\n</html>\n");
}

#else  // BUILDFLAG(IS_CHROMEOS)

void AppendBody(std::string *output) {
  output->append("</head>\n<body>\n");
}

void AppendFooter(std::string *output) {
  output->append("</body>\n</html>\n");
}

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace about_ui

using about_ui::AppendHeader;
using about_ui::AppendBody;
using about_ui::AppendFooter;

namespace {

bool CompareConfigInfos(const content::WebUIConfigInfo& config1,
                        const content::WebUIConfigInfo& config2) {
  // Schemes must be either chrome:// or chrome-untrusted://
  CHECK(config1.origin.scheme() == content::kChromeUIScheme ||
        config1.origin.scheme() == content::kChromeUIUntrustedScheme);
  CHECK(config2.origin.scheme() == content::kChromeUIScheme ||
        config2.origin.scheme() == content::kChromeUIUntrustedScheme);
  // Sort chrome:// before chrome-untrusted://. If the schemes are not equal,
  // given the check above one must be chrome:// and one chrome-untrusted://.
  if (config1.origin.scheme() != config2.origin.scheme()) {
    return config1.origin.scheme() == content::kChromeUIScheme;
  }
  return config1.origin.host() < config2.origin.host();
}

std::string ChromeURLs(content::BrowserContext* browser_context) {
  std::string html;
  AppendHeader(&html, "Chrome URLs");
  AppendBody(&html);

  html += "<h2>List of Chrome URLs</h2>\n<ul>\n";
  const base::span<const base::cstring_view> hosts = chrome::ChromeURLHosts();
  std::vector<content::WebUIConfigInfo> infos;
  for (base::cstring_view host : hosts) {
    GURL url(base::StrCat(
        {content::kChromeUIScheme, url::kStandardSchemeSeparator, host}));
    infos.push_back({.origin = url::Origin::Create(url), .enabled = true});
  }

  // Add any extra hosts found in the config map
  auto& map = content::WebUIConfigMap::GetInstance();
  for (const content::WebUIConfigInfo& config_info :
       map.GetWebUIConfigList(browser_context)) {
    if (config_info.origin.scheme() == content::kChromeUIUntrustedScheme ||
        std::find(hosts.begin(), hosts.end(), config_info.origin.host()) ==
            hosts.end()) {
      infos.push_back(config_info);
    }
  }

  // Sort the URLs.
  std::sort(infos.begin(), infos.end(), &CompareConfigInfos);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const bool is_lacros_primary = about_ui::isLacrosPrimaryOrCurrentBrowser();
  // If Lacros is active, the user can navigate by hand to os:// URL's but
  // internally we will still navigate to chrome:// URL's. Note also that
  // only a subset of URLs might be available in this mode - so we have to
  // make sure that only allowed URLs are being presented.
  if (is_lacros_primary) {
    auto* WebUiControllerFactory = ChromeWebUIControllerFactory::GetInstance();
    for (base::cstring_view host : hosts) {
      // TODO(crbug.com/40805730): The refactor should make sure that the
      // provided list can be shown as is without filtering.
      std::string chrome_url = base::StrCat({"chrome://", host});
      std::string os_url = base::StrCat({"os://", host});
      if (WebUiControllerFactory->CanHandleUrl(GURL(os_url)) ||
          WebUiControllerFactory->CanHandleUrl(GURL(chrome_url))) {
        html += base::StrCat(
            {"<li><a href='", chrome_url, "/'>", os_url, "</a></li>\n"});
      }
    }
  } else {
#else
  {
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    for (const content::WebUIConfigInfo& info : infos) {
      std::string host = info.origin.host();
      std::string scheme = info.origin.scheme();
      std::string url =
          base::StrCat({scheme, url::kStandardSchemeSeparator, host});
      html +=
          info.enabled
              ? base::StrCat({"<li><a href='", url, "/'>", url, "</a></li>\n"})
              : base::StrCat({"<li>", url, "</li>\n"});
    }

    html +=
        "</ul><a id=\"internals\"><h2>List of chrome://internals "
        "pages</h2></a>\n<ul>\n";
    const base::span<const base::cstring_view> internals_paths =
        chrome::ChromeInternalsURLPaths();
    std::vector<std::string_view> sorted_internals_paths = base::ToVector(
        internals_paths,
        [](base::cstring_view v) -> std::string_view { return v; });
    std::ranges::sort(sorted_internals_paths);
    for (const std::string_view path : sorted_internals_paths) {
      html += base::StrCat({"<li><a href='chrome://internals/", path,
                            "'>chrome://internals/", path, "</a></li>\n"});
    }
  }

  html += "</ul>\n<h2>For Debug</h2>\n"
      "<p>The following pages are for debugging purposes only. Because they "
      "crash or hang the renderer, they're not linked directly; you can type "
      "them into the address bar if you need them.</p>\n<ul>";
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // If Lacros is active, the user can navigate by hand to os:// URL's but
  // internally we will still navigate to chrome:// URL's. Note also that
  // only a subset of URLs might be available in this mode - so we have to
  // make sure that only allowed URLs are being presented.
  if (is_lacros_primary) {
    auto* WebUiControllerFactory = ChromeWebUIControllerFactory::GetInstance();
    for (base::cstring_view url : chrome::ChromeDebugURLs()) {
      // TODO(crbug.com/40805730): The refactor should make sure that the
      // provided list can be shown as is without filtering.
      const std::string host = GURL(url).host();
      if (WebUiControllerFactory->CanHandleUrl(GURL("os://" + host)) ||
          WebUiControllerFactory->CanHandleUrl(GURL("chrome://" + host))) {
        html += base::StrCat({"<li>os://", host, "</li>\n"});
      }
    }
  } else {
#else
  {
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    for (base::cstring_view url : chrome::ChromeDebugURLs()) {
      html += base::StrCat({"<li>", url, "</li>\n"});
    }
  }
  html += "</ul>\n";

  AppendFooter(&html);
  return html;
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OPENBSD)
std::string AboutLinuxProxyConfig() {
  std::string data;
  AppendHeader(&data,
               l10n_util::GetStringUTF8(IDS_ABOUT_LINUX_PROXY_CONFIG_TITLE));
  data.append("<style>body { max-width: 70ex; padding: 2ex 5ex; }</style>");
  AppendBody(&data);
  base::FilePath binary = base::CommandLine::ForCurrentProcess()->GetProgram();
  data.append(
      l10n_util::GetStringFUTF8(IDS_ABOUT_LINUX_PROXY_CONFIG_BODY,
                                l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
                                base::ASCIIToUTF16(binary.BaseName().value())));
  AppendFooter(&data);
  return data;
}
#endif

}  // namespace

AboutUIConfigBase::AboutUIConfigBase(std::string_view host)
    : DefaultWebUIConfig(content::kChromeUIScheme, host) {}

ChromeURLsUIConfig::ChromeURLsUIConfig()
    : AboutUIConfigBase(chrome::kChromeUIChromeURLsHost) {}

CreditsUIConfig::CreditsUIConfig()
    : AboutUIConfigBase(chrome::kChromeUICreditsHost) {}

#if !BUILDFLAG(IS_ANDROID)
TermsUIConfig::TermsUIConfig()
    : AboutUIConfigBase(chrome::kChromeUITermsHost) {}
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OPENBSD)
LinuxProxyConfigUI::LinuxProxyConfigUI()
    : AboutUIConfigBase(chrome::kChromeUILinuxProxyConfigHost) {}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
OSCreditsUI::OSCreditsUI()
    : AboutUIConfigBase(chrome::kChromeUIOSCreditsHost) {}

BorealisCreditsUI::BorealisCreditsUI()
    : AboutUIConfigBase(chrome::kChromeUIBorealisCreditsHost) {}

CrostiniCreditsUI::CrostiniCreditsUI()
    : AboutUIConfigBase(chrome::kChromeUICrostiniCreditsHost) {}
#endif

// AboutUIHTMLSource ----------------------------------------------------------

AboutUIHTMLSource::AboutUIHTMLSource(const std::string& source_name,
                                     Profile* profile)
    : source_name_(source_name),
      profile_(profile) {}

AboutUIHTMLSource::~AboutUIHTMLSource() {}

std::string AboutUIHTMLSource::GetSource() {
  return source_name_;
}

void AboutUIHTMLSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  // TODO(crbug.com/40050262): Simplify usages of |path| since |url| is
  // available.
  const std::string path = content::URLDataSource::URLToRequestPath(url);
  std::string response;
  // Add your data source here, in alphabetical order.
  if (source_name_ == chrome::kChromeUIChromeURLsHost) {
    response = ChromeURLs(profile_);
  } else if (source_name_ == chrome::kChromeUICreditsHost) {
    int idr = IDR_ABOUT_UI_CREDITS_HTML;
    if (path == kCreditsJsPath)
      idr = IDR_ABOUT_UI_CREDITS_JS;
    else if (path == kCreditsCssPath)
      idr = IDR_ABOUT_UI_CREDITS_CSS;
    if (idr == IDR_ABOUT_UI_CREDITS_HTML) {
      response = about_ui::GetCredits(true /*include_scripts*/);
    } else {
      response =
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(idr);
    }
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OPENBSD)
  } else if (source_name_ == chrome::kChromeUILinuxProxyConfigHost) {
    response = AboutLinuxProxyConfig();
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (source_name_ == chrome::kChromeUIOSCreditsHost ||
             source_name_ == chrome::kChromeUICrostiniCreditsHost ||
             source_name_ == chrome::kChromeUIBorealisCreditsHost) {
    int idr = IDR_ABOUT_UI_CREDITS_HTML;
    if (path == kCreditsJsPath) {
      idr = IDR_ABOUT_UI_CREDITS_JS;
    } else if (path == kCreditsCssPath) {
      idr = IDR_ABOUT_UI_CREDITS_CSS;
    }
    if (idr == IDR_ABOUT_UI_CREDITS_HTML) {
      if (source_name_ == chrome::kChromeUIOSCreditsHost) {
        ChromeOSCreditsHandler::Start(path, std::move(callback),
                                      os_credits_prefix_);
      } else if (source_name_ == chrome::kChromeUICrostiniCreditsHost) {
        CrostiniCreditsHandler::Start(profile(), path, std::move(callback));
      } else if (source_name_ == chrome::kChromeUIBorealisCreditsHost) {
        HandleBorealisCredits(profile(), std::move(callback));
      } else {
        NOTREACHED_IN_MIGRATION();
      }
      return;
    }

    response =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(idr);
#endif
#if !BUILDFLAG(IS_ANDROID)
  } else if (source_name_ == chrome::kChromeUITermsHost) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (!path.empty()) {
      ChromeOSTermsHandler::Start(path, std::move(callback));
      return;
    }
#endif
    response =
        ui::ResourceBundle::GetSharedInstance().LoadLocalizedResourceString(
            IDS_TERMS_HTML);
#endif
  }

  FinishDataRequest(response, std::move(callback));
}

void AboutUIHTMLSource::FinishDataRequest(
    const std::string& html,
    content::URLDataSource::GotDataCallback callback) {
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(html));
}

std::string AboutUIHTMLSource::GetMimeType(const GURL& url) {
  const std::string_view path = url.path_piece().substr(1);
  if (path == kCreditsJsPath || path == kStatsJsPath ||
      path == kStringsJsPath) {
    return "application/javascript";
  }

  if (path == kCreditsCssPath) {
    return "text/css";
  }

  return "text/html";
}

std::string AboutUIHTMLSource::GetAccessControlAllowOriginForOrigin(
    const std::string& origin) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Allow chrome://oobe to load chrome://terms via XHR.
  if (source_name_ == chrome::kChromeUITermsHost &&
      base::StartsWith(chrome::kChromeUIOobeURL, origin,
                       base::CompareCase::SENSITIVE)) {
    return origin;
  }
#endif
  return content::URLDataSource::GetAccessControlAllowOriginForOrigin(origin);
}

AboutUI::AboutUI(content::WebUI* web_ui, const GURL& url)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

#if !BUILDFLAG(IS_ANDROID)
  // Set up the chrome://theme/ source.
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
#endif

  content::URLDataSource::Add(
      profile, std::make_unique<AboutUIHTMLSource>(url.host(), profile));
}

#if BUILDFLAG(IS_CHROMEOS)

bool AboutUI::OverrideHandleWebUIMessage(const GURL& source_url,
                                         const std::string& message,
                                         const base::Value::List& args) {
  if (message != "crosUrlAboutRedirect")
    return false;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  lacros_url_handling::NavigateInAsh(GURL(chrome::kChromeUIAboutURL));
#else
  // Note: This will only be called by the UI when Lacros is available.
  DCHECK(crosapi::BrowserManager::Get());
  crosapi::BrowserManager::Get()->SwitchToTab(
      GURL(chrome::kChromeUIAboutURL),
      /*path_behavior=*/NavigateParams::RESPECT);
#endif
  return true;
}

#endif  // BUILDFLAG(IS_CHROMEOS)

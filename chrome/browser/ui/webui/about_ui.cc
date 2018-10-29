// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/about_ui.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/i18n/number_formatting.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/metrics/statistics_recorder.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/about_ui/credit_utils.h"
#include "components/grit/components_resources.h"
#include "components/strings/grit/components_locale_settings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/process_type.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/escape.h"
#include "net/base/filename_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "third_party/brotli/include/brotli/decode.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/webui/theme_source.h"
#endif

#if defined(OS_CHROMEOS)
#include <map>

#include "base/base64.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chromeos/system/statistics_provider.h"
#include "components/language/core/common/locale_util.h"
#endif

using content::BrowserThread;

namespace {

constexpr char kCreditsJsPath[] = "credits.js";
constexpr char kStatsJsPath[] = "stats.js";
constexpr char kStringsJsPath[] = "strings.js";

#if defined(OS_CHROMEOS)

constexpr char kKeyboardUtilsPath[] = "keyboard_utils.js";

// APAC region name.
constexpr char kApac[] = "apac";
// EMEA region name.
constexpr char kEmea[] = "emea";
// EU region name.
constexpr char kEu[] = "eu";

// List of countries that belong to APAC.
const char* const kApacCountries[] = {"au", "bd", "cn", "hk", "id", "in", "jp",
                                      "kh", "la", "lk", "mm", "mn", "my", "nz",
                                      "np", "ph", "sg", "th", "tw", "vn"};

// List of countries that belong to EMEA.
const char* const kEmeaCountries[] = {"na", "za", "am", "az", "ch", "eg", "ge",
                                      "il", "is", "ke", "kg", "li", "mk", "no",
                                      "rs", "ru", "tr", "tz", "ua", "ug", "za"};

// List of countries that belong to EU.
const char* const kEuCountries[] = {
    "at", "be", "bg", "cz", "dk", "es", "fi", "fr", "gb", "gr", "hr", "hu",
    "ie", "it", "lt", "lu", "lv", "nl", "pl", "pt", "ro", "se", "si", "sk"};

// Maps country to one of 3 regions: APAC, EMEA, EU.
typedef std::map<std::string, std::string> CountryRegionMap;

// Returns country to region map with EU, EMEA and APAC countries.
CountryRegionMap CreateCountryRegionMap() {
  CountryRegionMap region_map;
  for (size_t i = 0; i < base::size(kApacCountries); ++i) {
    region_map.emplace(kApacCountries[i], kApac);
  }

  for (size_t i = 0; i < base::size(kEmeaCountries); ++i) {
    region_map.emplace(kEmeaCountries[i], kEmea);
  }

  for (size_t i = 0; i < base::size(kEuCountries); ++i) {
    region_map.emplace(kEuCountries[i], kEu);
  }
  return region_map;
}

// Reads device region from VPD. Returns "us" in case of read or parsing errors.
std::string ReadDeviceRegionFromVpd() {
  std::string region = "us";
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  bool region_found =
      provider->GetMachineStatistic(chromeos::system::kRegionKey, &region);
  if (region_found) {
    // We only need the first part of the complex region codes like ca.ansi.
    std::vector<std::string> region_pieces = base::SplitString(
        region, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (!region_pieces.empty())
      region = region_pieces[0];
  } else {
    LOG(WARNING) << "Device region for Play Store ToS not found in VPD - "
                    "defaulting to US.";
  }
  return base::ToLowerASCII(region);
}

// Loads bundled terms of service contents (Eula, OEM Eula, Play Store Terms).
// The online version of terms is fetched in OOBE screen javascript. This is
// intentional because chrome://terms runs in a privileged webui context and
// should never load from untrusted places.
class ChromeOSTermsHandler
    : public base::RefCountedThreadSafe<ChromeOSTermsHandler> {
 public:
  static void Start(const std::string& path,
                    const content::URLDataSource::GotDataCallback& callback,
                    const base::FilePath& chromeos_assets_path) {
    scoped_refptr<ChromeOSTermsHandler> handler(
        new ChromeOSTermsHandler(path, callback, chromeos_assets_path));
    handler->StartOnUIThread();
  }

 private:
  friend class base::RefCountedThreadSafe<ChromeOSTermsHandler>;

  ChromeOSTermsHandler(const std::string& path,
                       const content::URLDataSource::GotDataCallback& callback,
                       const base::FilePath& chromeos_assets_path)
      : path_(path),
        callback_(callback),
        // Previously we were using "initial locale" http://crbug.com/145142
        locale_(g_browser_process->GetApplicationLocale()),
        chromeos_assets_path_(chromeos_assets_path) {}

  virtual ~ChromeOSTermsHandler() {}

  void StartOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (path_ == chrome::kOemEulaURLPath) {
      // Load local OEM EULA from the disk.
      base::PostTaskWithTraitsAndReply(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
          base::BindOnce(&ChromeOSTermsHandler::LoadOemEulaFileAsync, this),
          base::BindOnce(&ChromeOSTermsHandler::ResponseOnUIThread, this));
    } else if (path_ == chrome::kArcTermsURLPath) {
      // Load ARC++ terms from the file.
      base::PostTaskWithTraitsAndReply(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
          base::BindOnce(&ChromeOSTermsHandler::LoadArcTermsFileAsync, this),
          base::BindOnce(&ChromeOSTermsHandler::ResponseOnUIThread, this));
    } else if (path_ == chrome::kArcPrivacyPolicyURLPath) {
      // Load ARC++ privacy policy from the file.
      base::PostTaskWithTraitsAndReply(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
          base::BindOnce(&ChromeOSTermsHandler::LoadArcPrivacyPolicyFileAsync,
                         this),
          base::BindOnce(&ChromeOSTermsHandler::ResponseOnUIThread, this));
    } else {
      // Load local ChromeOS terms from the file.
      base::PostTaskWithTraitsAndReply(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
          base::BindOnce(&ChromeOSTermsHandler::LoadEulaFileAsync, this),
          base::BindOnce(&ChromeOSTermsHandler::ResponseOnUIThread, this));
    }
  }

  void LoadOemEulaFileAsync() {
    base::ScopedBlockingCall scoped_blocking_call(
        base::BlockingType::MAY_BLOCK);

    const chromeos::StartupCustomizationDocument* customization =
        chromeos::StartupCustomizationDocument::GetInstance();
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

  void LoadEulaFileAsync() {
    base::ScopedBlockingCall scoped_blocking_call(
        base::BlockingType::MAY_BLOCK);

    std::string file_path =
        base::StringPrintf(chrome::kEULAPathFormat, locale_.c_str());
    if (!base::ReadFileToString(base::FilePath(file_path), &contents_)) {
      // No EULA for given language - try en-US as default.
      file_path = base::StringPrintf(chrome::kEULAPathFormat, "en-US");
      if (!base::ReadFileToString(base::FilePath(file_path), &contents_)) {
        // File with EULA not found, ResponseOnUIThread will load EULA from
        // resources if contents_ is empty.
        contents_.clear();
      }
    }
  }

  void LoadArcPrivacyPolicyFileAsync() {
    base::ScopedBlockingCall scoped_blocking_call(
        base::BlockingType::MAY_BLOCK);

    for (const auto& locale : CreateArcLocaleLookupArray()) {
      auto path = CreateArcPrivacyPolicyPath(locale.c_str());
      std::string contents;
      if (base::ReadFileToString(path, &contents)) {
        base::Base64Encode(contents, &contents_);
        VLOG(1) << "Read offline Play Store privacy policy for: " << locale;
        return;
      }
      LOG(WARNING) << "Could not find offline Play Store privacy policy for: "
                   << locale;
    }
    LOG(ERROR) << "Failed to load offline Play Store privacy policy";
    contents_.clear();
  }

  void LoadArcTermsFileAsync() {
    base::ScopedBlockingCall scoped_blocking_call(
        base::BlockingType::MAY_BLOCK);

    for (const auto& locale : CreateArcLocaleLookupArray()) {
      auto path = CreateArcTermsPath(locale.c_str());
      std::string contents;
      if (base::ReadFileToString(CreateArcTermsPath(locale), &contents_)) {
        VLOG(1) << "Read offline Play Store terms for: " << locale;
        return;
      }
      LOG(WARNING) << "Could not find offline Play Store terms for: " << locale;
    }
    LOG(ERROR) << "Failed to load offline Play Store ToS";
    contents_.clear();
  }

  std::vector<std::string> CreateArcLocaleLookupArray() {
    // To get Play Store asset we look for the first locale match in the
    // following order:
    // * language and device region combination
    // * default region (APAC, EMEA, EU)
    // * en-US
    // Note: AMERICAS region defaults to en-US and to simplify it is not
    // included in the country region map.
    std::vector<std::string> locale_lookup_array;
    const std::string device_region = ReadDeviceRegionFromVpd();
    locale_lookup_array.push_back(base::StrCat(
        {base::ToLowerASCII(language::ExtractBaseLanguage(locale_)), "-",
         device_region}));

    const CountryRegionMap country_region_map = CreateCountryRegionMap();
    const auto region = country_region_map.find(device_region);
    if (region != country_region_map.end()) {
      locale_lookup_array.push_back(region->second.c_str());
    }

    locale_lookup_array.push_back("en-us");
    return locale_lookup_array;
  }

  base::FilePath CreateArcTermsPath(const std::string& locale) const {
    return chromeos_assets_path_.Append(
        base::StringPrintf(chrome::kArcTermsPathFormat, locale.c_str()));
  }

  base::FilePath CreateArcPrivacyPolicyPath(const std::string& locale) const {
    return chromeos_assets_path_.Append(base::StringPrintf(
        chrome::kArcPrivacyPolicyPathFormat, locale.c_str()));
  }

  void ResponseOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // If we fail to load Chrome OS EULA from disk, load it from resources.
    // Do nothing if OEM EULA or Play Store ToS load failed.
    if (contents_.empty() && path_.empty())
      contents_ = l10n_util::GetStringUTF8(IDS_TERMS_HTML);
    callback_.Run(base::RefCountedString::TakeString(&contents_));
  }

  // Path in the URL.
  const std::string path_;

  // Callback to run with the response.
  content::URLDataSource::GotDataCallback callback_;

  // Locale of the EULA.
  const std::string locale_;

  // Path to Chrome OS assets.
  const base::FilePath chromeos_assets_path_;

  // EULA contents that was loaded from file.
  std::string contents_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSTermsHandler);
};

class ChromeOSCreditsHandler
    : public base::RefCountedThreadSafe<ChromeOSCreditsHandler> {
 public:
  static void Start(const std::string& path,
                    const content::URLDataSource::GotDataCallback& callback) {
    scoped_refptr<ChromeOSCreditsHandler> handler(
        new ChromeOSCreditsHandler(path, callback));
    handler->StartOnUIThread();
  }

 private:
  friend class base::RefCountedThreadSafe<ChromeOSCreditsHandler>;

  ChromeOSCreditsHandler(
      const std::string& path,
      const content::URLDataSource::GotDataCallback& callback)
      : path_(path), callback_(callback) {}

  virtual ~ChromeOSCreditsHandler() {}

  void StartOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (path_ == kKeyboardUtilsPath) {
      contents_ = ui::ResourceBundle::GetSharedInstance()
                      .GetRawDataResource(IDR_KEYBOARD_UTILS_JS)
                      .as_string();
      ResponseOnUIThread();
      return;
    }
    // Load local Chrome OS credits from the disk.
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::Bind(&ChromeOSCreditsHandler::LoadCreditsFileAsync, this),
        base::Bind(&ChromeOSCreditsHandler::ResponseOnUIThread, this));
  }

  void LoadCreditsFileAsync() {
    base::FilePath credits_file_path(chrome::kChromeOSCreditsPath);
    if (!base::ReadFileToString(credits_file_path, &contents_)) {
      // File with credits not found, ResponseOnUIThread will load credits
      // from resources if contents_ is empty.
      contents_.clear();
    }
  }

  void ResponseOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // If we fail to load Chrome OS credits from disk, load it from resources.
    if (contents_.empty() && path_ != kKeyboardUtilsPath) {
      contents_ = ui::ResourceBundle::GetSharedInstance()
                      .GetRawDataResource(IDR_OS_CREDITS_HTML)
                      .as_string();
    }
    callback_.Run(base::RefCountedString::TakeString(&contents_));
  }

  // Path in the URL.
  const std::string path_;

  // Callback to run with the response.
  content::URLDataSource::GotDataCallback callback_;

  // Chrome OS credits contents that was loaded from file.
  std::string contents_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSCreditsHandler);
};

class LinuxCreditsHandler
    : public base::RefCountedThreadSafe<LinuxCreditsHandler> {
 public:
  static void Start(const std::string& path,
                    const content::URLDataSource::GotDataCallback& callback) {
    scoped_refptr<LinuxCreditsHandler> handler(
        new LinuxCreditsHandler(path, callback));
    handler->StartOnUIThread();
  }

 private:
  friend class base::RefCountedThreadSafe<LinuxCreditsHandler>;

  LinuxCreditsHandler(const std::string& path,
                      const content::URLDataSource::GotDataCallback& callback)
      : path_(path), callback_(callback) {}

  virtual ~LinuxCreditsHandler() {}

  void StartOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (path_ == kKeyboardUtilsPath) {
      contents_ = ui::ResourceBundle::GetSharedInstance()
                      .GetRawDataResource(IDR_KEYBOARD_UTILS_JS)
                      .as_string();
      ResponseOnUIThread();
      return;
    }
    // Load local Linux credits from the disk.
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::Bind(&LinuxCreditsHandler::LoadLinuxCreditsFileAsync, this),
        base::Bind(&LinuxCreditsHandler::ResponseOnUIThread, this));
  }

  void LoadLinuxCreditsFileAsync() {
    base::FilePath credits_file_path(chrome::kLinuxCreditsPath);
    if (!base::ReadFileToString(credits_file_path, &contents_)) {
      // File with credits not found, ResponseOnUIThread will load credits
      // from resources if contents_ is empty.
      contents_.clear();
    }
  }

  void ResponseOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // If we fail to load Linux credits from disk, load the placeholder from
    // resources.
    // TODO(rjwright): Add a linux-specific placeholder in resources.
    if (contents_.empty() && path_ != kKeyboardUtilsPath) {
      contents_ = ui::ResourceBundle::GetSharedInstance()
                      .GetRawDataResource(IDR_OS_CREDITS_HTML)
                      .as_string();
    }
    callback_.Run(base::RefCountedString::TakeString(&contents_));
  }

  // Path in the URL.
  const std::string path_;

  // Callback to run with the response.
  content::URLDataSource::GotDataCallback callback_;

  // Linux credits contents that was loaded from file.
  std::string contents_;

  DISALLOW_COPY_AND_ASSIGN(LinuxCreditsHandler);
};
#endif

}  // namespace

// Individual about handlers ---------------------------------------------------

namespace about_ui {

void AppendHeader(std::string* output, int refresh,
                  const std::string& unescaped_title) {
  output->append("<!DOCTYPE HTML>\n<html>\n<head>\n");
  if (!unescaped_title.empty()) {
    output->append("<title>");
    output->append(net::EscapeForHTML(unescaped_title));
    output->append("</title>\n");
  }
  output->append("<meta charset='utf-8'>\n");
  if (refresh > 0) {
    output->append("<meta http-equiv='refresh' content='");
    output->append(base::IntToString(refresh));
    output->append("'/>\n");
  }
}

void AppendBody(std::string *output) {
  output->append("</head>\n<body>\n");
}

void AppendFooter(std::string *output) {
  output->append("</body>\n</html>\n");
}

}  // namespace about_ui

using about_ui::AppendHeader;
using about_ui::AppendBody;
using about_ui::AppendFooter;

namespace {

std::string ChromeURLs() {
  std::string html;
  AppendHeader(&html, 0, "Chrome URLs");
  AppendBody(&html);
  html += "<h2>List of Chrome URLs</h2>\n<ul>\n";
  std::vector<std::string> hosts(
      chrome::kChromeHostURLs,
      chrome::kChromeHostURLs + chrome::kNumberOfChromeHostURLs);
  std::sort(hosts.begin(), hosts.end());
  for (std::vector<std::string>::const_iterator i = hosts.begin();
       i != hosts.end(); ++i)
    html += "<li><a href='chrome://" + *i + "/'>chrome://" + *i + "</a></li>\n";
  html += "</ul>\n<h2>For Debug</h2>\n"
      "<p>The following pages are for debugging purposes only. Because they "
      "crash or hang the renderer, they're not linked directly; you can type "
      "them into the address bar if you need them.</p>\n<ul>";
  for (size_t i = 0; i < chrome::kNumberOfChromeDebugURLs; i++)
    html += "<li>" + std::string(chrome::kChromeDebugURLs[i]) + "</li>\n";
  html += "</ul>\n";
  AppendFooter(&html);
  return html;
}

#if defined(OS_LINUX) || defined(OS_OPENBSD)
std::string AboutLinuxProxyConfig() {
  std::string data;
  AppendHeader(&data, 0,
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

// AboutUIHTMLSource ----------------------------------------------------------

AboutUIHTMLSource::AboutUIHTMLSource(const std::string& source_name,
                                     Profile* profile)
    : source_name_(source_name),
      profile_(profile) {}

AboutUIHTMLSource::~AboutUIHTMLSource() {}

std::string AboutUIHTMLSource::GetSource() const {
  return source_name_;
}

void AboutUIHTMLSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string response;
  // Add your data source here, in alphabetical order.
  if (source_name_ == chrome::kChromeUIChromeURLsHost) {
    response = ChromeURLs();
  } else if (source_name_ == chrome::kChromeUICreditsHost) {
    int idr = IDR_ABOUT_UI_CREDITS_HTML;
    if (path == kCreditsJsPath)
      idr = IDR_ABOUT_UI_CREDITS_JS;
#if defined(OS_CHROMEOS)
    else if (path == kKeyboardUtilsPath)
      idr = IDR_KEYBOARD_UTILS_JS;
#endif
    if (idr == IDR_ABOUT_UI_CREDITS_HTML) {
      response = about_ui::GetCredits(true /*include_scripts*/);
    } else {
      response = ui::ResourceBundle::GetSharedInstance()
                     .GetRawDataResource(idr)
                     .as_string();
    }
#if defined(OS_LINUX) || defined(OS_OPENBSD)
  } else if (source_name_ == chrome::kChromeUILinuxProxyConfigHost) {
    response = AboutLinuxProxyConfig();
#endif
#if defined(OS_CHROMEOS)
  } else if (source_name_ == chrome::kChromeUIOSCreditsHost) {
    ChromeOSCreditsHandler::Start(path, callback);
    return;
  } else if (source_name_ == chrome::kChromeUILinuxCreditsHost) {
    LinuxCreditsHandler::Start(path, callback);
    return;
#endif
#if !defined(OS_ANDROID)
  } else if (source_name_ == chrome::kChromeUITermsHost) {
#if defined(OS_CHROMEOS)
    std::string assets_dir = chromeos_assets_dir_for_tests_.empty()
                                 ? chrome::kChromeOSAssetPath
                                 : chromeos_assets_dir_for_tests_;
    ChromeOSTermsHandler::Start(path, callback, base::FilePath(assets_dir));
    return;
#else
    response = l10n_util::GetStringUTF8(IDS_TERMS_HTML);
#endif
#endif
  }

  FinishDataRequest(response, callback);
}

void AboutUIHTMLSource::FinishDataRequest(
    const std::string& html,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string html_copy(html);
  callback.Run(base::RefCountedString::TakeString(&html_copy));
}

std::string AboutUIHTMLSource::GetMimeType(const std::string& path) const {
  if (path == kCreditsJsPath     ||
#if defined(OS_CHROMEOS)
      path == kKeyboardUtilsPath ||
#endif
      path == kStatsJsPath       ||
      path == kStringsJsPath) {
    return "application/javascript";
  }
  return "text/html";
}

bool AboutUIHTMLSource::ShouldAddContentSecurityPolicy() const {
#if defined(OS_CHROMEOS)
  if (source_name_ == chrome::kChromeUIOSCreditsHost ||
      source_name_ == chrome::kChromeUILinuxCreditsHost) {
    return false;
  }
#endif
  return content::URLDataSource::ShouldAddContentSecurityPolicy();
}

std::string AboutUIHTMLSource::GetAccessControlAllowOriginForOrigin(
    const std::string& origin) const {
#if defined(OS_CHROMEOS)
  // Allow chrome://oobe to load chrome://terms via XHR.
  if (source_name_ == chrome::kChromeUITermsHost &&
      base::StartsWith(chrome::kChromeUIOobeURL, origin,
                       base::CompareCase::SENSITIVE)) {
    return origin;
  }
#endif
  return content::URLDataSource::GetAccessControlAllowOriginForOrigin(origin);
}

AboutUI::AboutUI(content::WebUI* web_ui, const std::string& name)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

#if !defined(OS_ANDROID)
  // Set up the chrome://theme/ source.
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
#endif

  content::URLDataSource::Add(
      profile, std::make_unique<AboutUIHTMLSource>(name, profile));
}

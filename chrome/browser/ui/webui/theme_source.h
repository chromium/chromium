// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_THEME_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_THEME_SOURCE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/url_data_source.h"

class Profile;

class ThemeSource : public content::URLDataSource {
 public:
  explicit ThemeSource(Profile* profile);
  ThemeSource(Profile* profile, bool serve_untrusted);

  ThemeSource(const ThemeSource&) = delete;
  ThemeSource& operator=(const ThemeSource&) = delete;

  ~ThemeSource() override;

  // TODO(crbug.com/457618790): Move this into a profile service.
  static const char kThemeColorsCssUrl[];

  // Generates the CSS content for theme colors.
  //
  // This function parses the `sets` query parameter from `url` to determine
  // which color ID sets to include. It uses `color_provider` to resolve the
  // actual color values. `is_grayscale` and `is_baseline` are used to inject
  // metadata variables into the CSS header.
  //
  // Returns the generated CSS string on success, or std::nullopt if the URL is
  // invalid, e.g., missing or invalid `sets` parameter.
  static std::optional<std::string> GenerateColorsCss(
      const ui::ColorProvider& color_provider,
      const GURL& url,
      bool is_grayscale,
      bool is_baseline);

  // content::URLDataSource implementation.
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const GURL& url) override;
  bool AllowCaching() override;
  bool ShouldServiceRequest(const GURL& url,
                            content::BrowserContext* browser_context,
                            int render_process_id) override;
  std::string GetAccessControlAllowOriginForOrigin(
      const std::string& origin) override;
  std::string GetContentSecurityPolicy(
      network::mojom::CSPDirectiveName directive) override;

 private:
  // Fetches and sends the theme bitmap.
  void SendThemeBitmap(content::URLDataSource::GotDataCallback callback,
                       int resource_id,
                       float scale);

  // Used in place of SendThemeBitmap when the desired scale is larger than
  // what the resource bundle supports.  This can rescale the provided bitmap up
  // to the desired size.
  void SendThemeImage(content::URLDataSource::GotDataCallback callback,
                      int resource_id,
                      float scale);

  // Generates and sends a CSS stylesheet with colors from the |ColorProvider|.
  // A 'sets' query parameter must be specified to indicate which colors should
  // be in the stylesheet. e.g chrome://theme/colors.css?sets=ui,chrome
  void SendColorsCss(const GURL& url,
                     const content::WebContents::Getter& wc_getter,
                     content::URLDataSource::GotDataCallback callback);

#if BUILDFLAG(IS_CHROMEOS)
  void SendTypographyCss(content::URLDataSource::GotDataCallback callback);
#endif

  // The profile this object was initialized with.
  raw_ptr<Profile, FlakyDanglingUntriaged> profile_;

  // Whether this source services chrome-unstrusted://theme.
  bool serve_untrusted_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_THEME_SOURCE_H_

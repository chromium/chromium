// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "chrome/renderer/plugins/plugin_uma.h"
#include "media/media_buildflags.h"
#include "ppapi/buildflags/buildflags.h"

class PluginUMATest : public testing::Test {
 public:
  static void ExpectPluginType(
      PluginUMAReporter::PluginType expected_plugin_type,
      const std::string& plugin_mime_type,
      const GURL& plugin_src) {
    EXPECT_EQ(expected_plugin_type,
              PluginUMAReporter::GetInstance()->GetPluginType(plugin_mime_type,
                                                              plugin_src));
  }
};

TEST_F(PluginUMATest, WindowsMediaPlayer) {
  ExpectPluginType(PluginUMAReporter::WINDOWS_MEDIA_PLAYER,
                   "application/x-mplayer2",
                   GURL("file://some_file.mov"));
  ExpectPluginType(PluginUMAReporter::UNSUPPORTED_MIMETYPE,
                   "application/x-mplayer2-some_sufix",
                   GURL("file://some_file.mov"));
  ExpectPluginType(PluginUMAReporter::UNSUPPORTED_MIMETYPE,
                   "some-prefix-application/x-mplayer2",
                   GURL("file://some_file.mov"));
}

TEST_F(PluginUMATest, Silverlight) {
  ExpectPluginType(PluginUMAReporter::SILVERLIGHT,
                   "application/x-silverlight",
                   GURL("aaaa"));
  ExpectPluginType(PluginUMAReporter::SILVERLIGHT,
                   "application/x-silverlight-some-sufix",
                   GURL("aaaa"));
  ExpectPluginType(PluginUMAReporter::UNSUPPORTED_MIMETYPE,
                   "some-prefix-application/x-silverlight",
                   GURL("aaaa"));
}

TEST_F(PluginUMATest, RealPlayer) {
  ExpectPluginType(
      PluginUMAReporter::REALPLAYER, "audio/x-pn-realaudio", GURL("some url"));
  ExpectPluginType(PluginUMAReporter::REALPLAYER,
                   "audio/x-pn-realaudio-some-sufix",
                   GURL("some url"));
  ExpectPluginType(PluginUMAReporter::UNSUPPORTED_MIMETYPE,
                   "some-prefix-audio/x-pn-realaudio",
                   GURL("some url"));
}

TEST_F(PluginUMATest, Java) {
  ExpectPluginType(
      PluginUMAReporter::JAVA, "application/x-java-applet", GURL("some url"));
  ExpectPluginType(PluginUMAReporter::JAVA,
                   "application/x-java-applet-some-sufix",
                   GURL("some url"));
  ExpectPluginType(PluginUMAReporter::JAVA,
                   "some-prefix-application/x-java-applet-sufix",
                   GURL("some url"));
}

TEST_F(PluginUMATest, QuickTime) {
  ExpectPluginType(
      PluginUMAReporter::QUICKTIME, "video/quicktime", GURL("some url"));
  ExpectPluginType(PluginUMAReporter::UNSUPPORTED_MIMETYPE,
                   "video/quicktime-sufix",
                   GURL("some url"));
  ExpectPluginType(PluginUMAReporter::UNSUPPORTED_MIMETYPE,
                   "prefix-video/quicktime",
                   GURL("some url"));
}

TEST_F(PluginUMATest, BrowserPlugin) {
  ExpectPluginType(PluginUMAReporter::BROWSER_PLUGIN,
                   "application/browser-plugin",
                   GURL("some url"));
  ExpectPluginType(PluginUMAReporter::UNSUPPORTED_MIMETYPE,
                   "application/browser-plugin-sufix",
                   GURL("some url"));
  ExpectPluginType(PluginUMAReporter::UNSUPPORTED_MIMETYPE,
                   "prefix-application/browser-plugin",
                   GURL("some url"));
}

TEST_F(PluginUMATest, BySrcExtension) {
  ExpectPluginType(
      PluginUMAReporter::QUICKTIME, std::string(), GURL("file://file.mov"));

  // When plugin's mime type is given, we don't check extension.
  ExpectPluginType(PluginUMAReporter::UNSUPPORTED_MIMETYPE,
                   "unknown-plugin",
                   GURL("http://file.mov"));

  ExpectPluginType(PluginUMAReporter::WINDOWS_MEDIA_PLAYER,
                   std::string(),
                   GURL("file://file.asx"));
  ExpectPluginType(
      PluginUMAReporter::REALPLAYER, std::string(), GURL("file://file.rm"));
  ExpectPluginType(PluginUMAReporter::QUICKTIME,
                   std::string(),
                   GURL("http://aaa/file.mov?x=aaaa&y=b#c"));
  ExpectPluginType(PluginUMAReporter::QUICKTIME,
                   std::string(),
                   GURL("http://file.mov?x=aaaa&y=b#c"));

  ExpectPluginType(PluginUMAReporter::UNSUPPORTED_EXTENSION,
                   std::string(),
                   GURL("http://file.unknown_extension"));
  ExpectPluginType(
      PluginUMAReporter::UNSUPPORTED_EXTENSION, std::string(), GURL("http://"));
  ExpectPluginType(
      PluginUMAReporter::UNSUPPORTED_EXTENSION, std::string(), GURL("mov"));
}

TEST_F(PluginUMATest, CaseSensitivity) {
  ExpectPluginType(
      PluginUMAReporter::QUICKTIME, "video/QUICKTIME", GURL("http://file.aaa"));
  ExpectPluginType(
      PluginUMAReporter::QUICKTIME, std::string(), GURL("http://file.MoV"));
}

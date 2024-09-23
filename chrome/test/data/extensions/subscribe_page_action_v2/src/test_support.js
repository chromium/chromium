// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides a default implementation for the sanitizer (a pass-through), so that
// tests can be run on the extension directly (without having to Closure compile
// it first).

var goog = {
  dom: {
    safe: {
      setAnchorHref: function(element, href) {
        element.href = href;
      },
      setInnerHtml: function(element, html) {
        element.innerHTML = html;
      }
    }
  },
  html: {
    sanitizer: {
      HtmlSanitizer: {
        sanitize: function(data) {
          return data;
        }
      }
    },
    SafeUrl: {
      sanitize: function(data) {
        return data;
      }
    }
  },

  provide: function(namespace) {},
  require: function(namespace) {}
};

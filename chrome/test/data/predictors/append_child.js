// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var img = document.createElement("img");
img.src = "/handled-by-test/image.png"
document.body.appendChild(img);

var link = document.createElement("link");
link.rel = "stylesheet";
link.href = "/handled-by-test/style.css";
document.head.appendChild(link);

var script = document.createElement("script");
script.src = "/handled-by-test/script.js";
document.body.appendChild(script);

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var fullscreen = matchMedia( '(display-mode: fullscreen)' );
var p = document.createElement("p");
document.body.appendChild(p);
if (fullscreen.matches) {
  p.innerHTML = "(display-mode: fullscreen) matches.";
  p.style.color = "green";
} else {
  p.innerHTML = "(display-mode: fullscreen) does not match.";
  p.style.color = "red";
}

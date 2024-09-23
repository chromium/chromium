This directory contains mechanisms for Remote Cocoa, which is the system that
bridges Cocoa objects with their Chromium C++ counterparts, which can be either
in-process (the browser) or out-of-process (PWA app shims).
 
One such example is bridging Cocoa NSWindows and NSViews with their Chromium
Views counterparts, but Remote Cocoa is not exclusive to views.
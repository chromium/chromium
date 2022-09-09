// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PDF_ADOBE_READER_INFO_WIN_H_
#define CHROME_BROWSER_UI_PDF_ADOBE_READER_INFO_WIN_H_

// Returns true if Adobe Reader or Adobe Acrobat is the default viewer for the
// .pdf extension.
bool IsAdobeReaderDefaultPDFViewer();

// If Adobe Reader or Adobe Acrobat is program associated with the .pdf viewer,
// return true if the executable is up to date.
// If Reader/Acrobat is not the default .pdf handler, return false.
// This function does blocking I/O, since it needs to read from the disk.
bool IsAdobeReaderUpToDate();

#endif  // CHROME_BROWSER_UI_PDF_ADOBE_READER_INFO_WIN_H_

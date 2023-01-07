#!/usr/bin/perl -w
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Technically, it's a third party.
#
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Generate html files that will be used test MIME type handling using
# the layout test framework.

use strict;
use Switch;  # used for switch-case program structure

my $arg_count = $#ARGV + 1;

# Make sure that only one command line argument is provided
if ($arg_count ne 1) {
  print "Usage:\n  generate_mime_tests.pl < target_path >\n  target_path".
    " -  path where the generated tests are to be placed";
  exit;
}

# Obtain target path from the command line
my $target_path = $ARGV[0];

# Set relative path of the page to be requested
my $root = "resources/getpage.pl?";

# Variables used in the script
my $content_type;
my $parameter;
my $source;  # Temporary variable to hold source path
my $count;  # Used to generate number appended to filenames
my $current_expected = "";  # Temporary storage for expected result
my $query_description;

# Number of tests each content type goes through, also determines the half
# size of the expected results matrix
my $test_set_size = 14;

# List of HTTP content types to be used in generating test HTMLs
my @content_type = ("NULL",
                "text/plain",
                "text/html",
                "image/gif",
                "image/bmp",
                "image/tif",
                "image/png",
                "image/jpg",
                "application/x-shockwave-flash");

# List of URL query parameters to be used in generating test HTMLs
my @parameter = ("tag=img&content=type_gif.gif",
              "tag=img&content=type_bmp.bmp",
              "tag=img&content=type_tif.tif",
              "tag=img&content=type_png.png",
              "tag=img&content=type_jpg.jpg",
              "tag=img&content=type_txt.txt",
              "tag=embed&content=type_swf.swf",
              "switch=nohtml&content=type_gif.gif",
              "switch=nohtml&content=type_bmp.bmp",
              "switch=nohtml&content=type_tif.tif",
              "switch=nohtml&content=type_png.png",
              "switch=nohtml&content=type_jpg.jpg",
              "switch=nohtml&content=type_txt.txt",
              "switch=nohtml&content=type_swf.swf");

# Matrix with expected results of all tests.
# The matrix rows have four parts 
#  1. iframe with a html
#  2. iframe without a html
#  3. content within a html page
#  4. content without a html
# Each part has the same sequence of column headers
my @expected = (
# gif   bmp    tif    png    jpg    txt    swf
#   gif    bmp    tif     png    jpg      txt    swf
#     gif    bmp    tif    png    jpg    txt    swf
#       gif     bmp    tif     png     jpg     txt    swf
                                                               # NULL
  "html","html","html","html","html","html","html",            # iframe html
    "void","void","image","void","void",  "text","text",       # iframe no html
      "html","html","html","html","html","html","html",        # html
        "image","void","image","image","image","text","text",  # no html

                                                               # text/plain
  "html","html","html","html","html","html","html",            # iframe html
    "void","void","image","void","void",  "text","text",       # iframe no html
      "html","html","html","html","html","html","html",        # html
        "image","void","image","image","image","text","text",  # no html

                                                               # text/html
  "rs",  "rf",  "rf",  "rs",  "rs",  "rf",  "rs",              # iframe html
    "text","text","text","text", "text",  "text","text",       # iframe no html
      "rs",  "rf",  "rf",   "rs", "rs",  "rf",  "rs",          # html
        "text", "text","text", "text", "text", "text","text",  # no html

                                                               # image/gif
  "void","void","void","void","void","void","void",            # iframe html
    "void","void","void","void", "void",  "void","void",       # iframe no html
      "void","void","void","void","void","void","void",        # html
        "image","void","void", "image","image","void","void",  # no html

                                                               # image/bmp
  "void","void","void","void","void","void","void",            # iframe html
    "void","void","void","void", "void",  "void","void",       # iframe no html
      "void","void","void","void","void","void","void",        # html
        "image","void","void", "image","image","void","void",  # no html

                                                               # image/tif
  "void","void","void","void","void","void","void",            # iframe html
    "void","void","void","void", "void",  "void","void",       # iframe no html
      "void","void","void","void","void","void","void",        # html
        "void", "void","void", "void", "void", "void","void",  # no html

                                                               # image/png
  "void","void","void","void","void","void","void",            # iframe html
    "void","void","void","void", "void",  "void","void",       # iframe no html
      "void","void","void","void","void","void","void",        # html
        "image","void","void", "image","image","void","void",  # no html

                                                               # image/jpg
  "void","void","void","void","void","void","void",            # iframe html
    "void","void","void","void", "void",  "void","void",       # iframe no html
      "void","void","void","void","void","void","void",        # html
        "image","void","void", "image","void", "void","void",  # no html

                                                               # application/x-shockwave-flash
  "void","void","void","void","void","void","void",            # iframe html
    "flash","void","void","flash","flash","void","flash",      # iframe no html
      "void","void","void","void","void","void","void",        # html
        "flash","void","void", "flash","flash","void","flash");# no html

# get_description()
#   Maps the expected word to an appropriate phrase.
#   Used to generated verbal descriptions for expected results of every test.
#   Input : expected result from the matrix
#   Output : returns the associated description
sub get_result_description
{
  switch ($_[0]) {
    case "void"  { return "NOTHING";}
    case "image" { return "an IMAGE";}
    case "text"  { return "simple TEXT";}
    case "html"  { return "an HTML as text";}
    case "flash" { return "a FLASH object"}
    case "rs"    { return "been RENDERED CORRECTLY";}
    case "rf"    { return "been RENDERED INCORRECTLY";}
    else         { return "UNKNOWN";}
  }
}

# get_query_description()
#   Maps the URL query to an appropriate phrase.
#   Used to generated verbal descriptions for URL queries of every test.
#   Input : URL query
#   Output : returns the associated description
sub get_query_description
{
  switch ($_[0]) {     
    case "tag=img&content=type_gif.gif" { return "HTML page with a GIF image";}
    case "tag=img&content=type_bmp.bmp" { return "HTML page with a BMP image";}
    case "tag=img&content=type_tif.tif" { return "HTML page with a TIF image";}
    case "tag=img&content=type_png.png" { return "HTML page with a PNG image";}
    case "tag=img&content=type_jpg.jpg" { return "HTML page with a JPEG image"}
    case "tag=img&content=type_txt.txt" { return "HTML page";}
    case "tag=embed&content=type_swf.swf" { return "an HTML page with a FLASH object";}
    case "switch=nohtml&content=type_gif.gif" { return "GIF image and no HTML";}
    case "switch=nohtml&content=type_bmp.bmp" { return "BMP image and no HTML";}
    case "switch=nohtml&content=type_tif.tif" { return "TIF image and no HTML";}
    case "switch=nohtml&content=type_png.png" { return "PNG image and no HTML";}
    case "switch=nohtml&content=type_jpg.jpg" { return "JPEG image and no HTML"}
    case "switch=nohtml&content=type_txt.txt" { return "simple TEXT and no HTML";}
    case "switch=nohtml&content=type_swf.swf" { return "FLASH object and no HTML";}
    else { return "UNKNOWN TYPE";}
  }
}

# This loop generates one HTML page with multiple iframes in it.
# Generated one iframe for every parameter of every content type.
my $iframe_index = 0;
foreach $content_type ( @content_type) {  
  my $infile = join "", "iframe/", $content_type, ".html";
  $infile =~ tr/\//_/;
  $infile = $target_path.$infile;

  open OUT, "> $infile" or die "Failed to open file $infile";
  print OUT "This HTML is used to test HTTP content-type \"$content_type\"".
    " by having multiple iframes render different types of content for the".
    " same HTTP content-type header.\n";
  print OUT "<script>\n  if(window.testRunner)\n    " .
    "window.testRunner.waitUntilDone();\n</script>\n";
  print OUT "<html>\n<body>\n<br>Well here are the frames !<br>\n";

  foreach $parameter ( @parameter ) {

    # Make sure to iterate only through the first half of the expected
    # results  matrix
    if (($iframe_index > 0) && (0 == ($iframe_index % $test_set_size))) {
        $iframe_index  += $test_set_size;
    }
    $current_expected = get_result_description($expected[$iframe_index++]);

    $source = join "", $root, "type=", $content_type, "&", $parameter;
    $query_description = get_query_description($parameter);
    print OUT "<br><br>This frame tests loading of a $query_description when the ".
      "HTTP content-type is set to \"$content_type\" .<br> Expected : This ",
      "iframe should have $current_expected .<br>\n<iframe src=\"$source\" ".
      "height=\"300\" width=\"500\"></iframe>\n\n";
  }

  print OUT "</body>\n</html>\n";
  print OUT "<script>\n  if(window.testRunner)\n    ".
    "testRunner.notifyDone();\n</script>";
  close OUT;
}

# This loop generates one HTML for every combination of content-type and
# parameter.
my $main_index = 0;
foreach $content_type ( @content_type) {
  $count = 0;
  foreach $parameter ( @parameter ) {
    $count++;

    # Make sure to iterate only through the second half of the expected
    # results  matrix
    if (0 == ($main_index % $test_set_size)) {
        $main_index  += $test_set_size;
    }

    $current_expected = get_result_description($expected[$main_index++]);

    my $infile = join "", "main/", $content_type, $count, ".html";
    $infile =~ tr/\//_/;
    $source = join "", $root, "type=", $content_type, "&", $parameter;
    $infile = $target_path.$infile;
    $query_description = get_query_description($parameter);

    open OUT, "> $infile" or die "Failed to open file $infile";
    print OUT "This tests loading of a $query_description when the HTTP content-type".
      " is set to \"$content_type\" .\n Expected : This page should have ".
      "$current_expected .\n\n";
    print OUT "<script>\n  window.location=\"$source\";\n</script>\n";
    close OUT;
  }
}

#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import unittest

import webforms_aggregator


class WebformsAggregatorTest(unittest.TestCase):
  """Unit tests for the webforms_aggregator module."""

  def setUp(self):
    self.cookie_file = 'test.cookie'
    self.url1 = 'http://www.google.com'
    self.url2 = 'http://www.macys.com'
    self.domain = 'google.com'
    self.url_file = tempfile.NamedTemporaryFile(suffix='.txt', delete=False)
    self.url_file.file.write(
        'URLs to crawl:\n%s\n%s\n' % (self.url1, self.url2))
    self.url_file.close()

  def tearDown(self):
    if os.path.isfile(self.cookie_file):
      os.unlink(self.cookie_file)
    if os.path.isfile(self.url_file.name):
      self.url_file.close()
      os.unlink(self.url_file.name)

  def testRetrieverDownloadsPage(self):
    """Verify the retriever can download a page."""
    r = webforms_aggregator.Retriever(self.url1, self.domain, self.cookie_file)
    self.assertTrue(r.Download(),
                    msg='Retriever could not download "%s"' % self.url1)

  def testCrawlerFindsRegPageFromUrl(self):
    """Verify that the crawler is able to find a reg page from the given URL."""
    c = webforms_aggregator.Crawler(self.url2)
    self.assertTrue(
        c.Run(), msg='Crawler could not find the reg page of "%s"' % self.url2)

  def testThreadedCrawlerFindsRegPageFromUrlsFile(self):
    """Verify the threaded crawler finds reg page from a file of URLs."""
    c = webforms_aggregator.ThreadedCrawler(self.url_file.name)
    self.assertNotEqual(
        c.Run(), -1,
        msg='Threaded crawler could not find the reg page from the URLs file')


if __name__ == '__main__':
  suite = unittest.TestLoader().loadTestsFromTestCase(
      WebformsAggregatorTest)
  unittest.TextTestRunner(verbosity=2).run(suite)

#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import tempfile
import unittest

# Same name as the aggregator module name.
import webforms_aggregator

logger = logging.getLogger(webforms_aggregator.__name__)
console = logging.StreamHandler()
logger.addHandler(console)

# Commenting out the following line will set logger level to default: WARNING
logger.setLevel(logging.INFO)


class WebformsAggregatorTest(unittest.TestCase):
  """Unit tests for the webforms_aggregator module."""
  PORT1 = 8002
  PORT2 = 8003

  HOME_CONTENT = """
    <!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" \
        "http://www.w3.org/TR/html4/loose.dtd">
    <html>
    <head>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
    <title>%s</title>
    </head>
    <body>
    <h1>%s</h1>
    <p>This is a mock site. Its mere purpose is to contribute towards testing \
        the aggregator crawler.</p>
    <ul>
     <li><a href="%s">page1</a></li>
     <li><a href="%s">page2</a></li>
     <li><a href="%s">page3</a></li>
    </ul>
    <hr>
    <p>
      <a href="%s">sign in</a>
    </p>
    </body>
    </html>
  """

  SIMPLE_PAGE_CONTENT = """
    <!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" \
        "http://www.w3.org/TR/html4/loose.dtd">
    <html>
    <head>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
    <title>%s</title>
    </head>
    <body>
    <h1>%s</h1>
    <p>%s</p>
    <ul>
     <li><a href="%s">%s</a></li>
     <li><a href="%s">%s</a></li>
    </ul>
    <hr>
    <p>
      <a href="%s">return to home page</a>
    </p>
    </body>
    </html>
  """

  SIGNIN_CONTENT = """
    <!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" \
        "http://www.w3.org/TR/html4/loose.dtd">
    <html>
    <head>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
    <title>%s</title>
    </head>
    <body>
    <h1>Sign in!</h1>
    <h3>%s</h3>
    <form>
      <label>User name: </label><input type="text"><br><br>
      <label>password: </label><input type="password"><br><br>
      <input type="submit" value="Sign in">
    </form>
    <hr>
    <p><a href="%s">return to home page</a></p>
    </body>
    </html>
  """

  REG_CONTENT = """
    <!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" \
        "http://www.w3.org/TR/html4/loose.dtd">
    <html>
    <head>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
    <title>%s</title>
    </head>
    <body>
    <h1>Create a user account!</h1>

    <h3>Enter your data below:</h3>
    <form method="get">
      <label>First name: </label><input type="text"><br><br>
      <label>Surname: </label><input type="text"><br><br>
      <label>User name: </label><input type="text"><br><br>
      <label>password: </label><input type="password"><br><br>
      <label>retype password: </label><input type="password"><br><br>
      <input type="submit" value="Register">
    </form>
    <hr>
    <p><a href="%s">return to home page</a></p>
    </body>
    </html>
  """

  def CreateMockSiteOne(self):
    """Site One has a registration form.
    """
    self.files['site1_home'] = 'site1_index.html'
    self.files['site1_page1'] = 'site1_page1.html'
    self.files['site1_page2'] = 'site1_page2.html'
    self.files['site1_page3'] = 'site1_page3.html'
    self.files['site1_signin'] = 'site1_signin.html'
    self.files['site1_reg'] = 'site1_register.html'

    file_content = {}
    file_content[self.files['site1_home']] = self.HOME_CONTENT % (
          'Site One home page', 'Welcome to site one. It has a reg page!',
          self.files['site1_page1'], self.files['site1_page2'],
          self.files['site1_page3'], self.files['site1_signin'])

    file_content[self.files['site1_page1']] = self.SIMPLE_PAGE_CONTENT % (
        'Site One page 1',
        'Page 1!', 'This is a useless page. It does almost nothing.',
        self.files['site1_page2'], 'page 2', self.files['site1_page3'],
        'page 3', self.files['site1_home'])

    file_content[self.files['site1_page2']] = self.SIMPLE_PAGE_CONTENT % (
        'Site One page 2', 'Page 2!',
        'This is another useless page. It does almost what the page 1 does.',
        self.files['site1_page1'], 'page 1', self.files['site1_page3'],
        'page 3', self.files['site1_home'])

    file_content[self.files['site1_page3']] = self.SIMPLE_PAGE_CONTENT % (
        'Site One page 3', 'Page 3!',
        "This is the last useless page. It doesn't do anything useful at all.",
        self.files['site1_page1'], 'page 1', self.files['site1_page2'],
        'page 2', self.files['site1_home'])

    file_content[self.files['site1_signin']] = self.SIGNIN_CONTENT % (
        'Site One signin',
        'If you don\'t have a user account click <a href="%s">here</a>.' \
            % self.files['site1_reg'],
        self.files['site1_home'])

    file_content[self.files['site1_reg']] = self.REG_CONTENT % (
        'Site One signin', self.files['site1_home'])

    for filename, content in file_content.iteritems():
      f = open(filename, 'w')
      try:
        f.write(content)
      finally:
        f.close()

  def CreateMockSiteTwo(self):
    """ Site Two has no registration page."""

    self.files['site2_home'] = 'site2_index.html'
    self.files['site2_page1'] = 'site2_page1.html'
    self.files['site2_page2'] = 'site2_page2.html'
    self.files['site2_page3'] = 'site2_page3.html'
    self.files['site2_signin'] = 'site2_signin.html'

    file_content = {}
    file_content[self.files['site2_home']] = self.HOME_CONTENT % (
          'Site Two home page', 'Welcome to site two. It has no reg page!',
          self.files['site2_page1'], self.files['site2_page2'],
          self.files['site2_page3'], self.files['site2_signin'])

    file_content[self.files['site2_page1']] = self.SIMPLE_PAGE_CONTENT % (
        'Site Two page 1',
        'Page 1!', 'This is a useless page. It does almost nothing.',
        self.files['site2_page2'], 'page 2', self.files['site2_page3'],
        'page 3', self.files['site2_home'])

    file_content[self.files['site2_page2']] = self.SIMPLE_PAGE_CONTENT % (
        'Site Two page 2', 'Page 2!',
        'This is another useless page. It does almost what the page 1 does.',
        self.files['site2_page1'], 'page 1', self.files['site2_page3'],
        'page 3', self.files['site2_home'])

    file_content[self.files['site2_page3']] = self.SIMPLE_PAGE_CONTENT % (
        'Site Two page 3', 'Page 3!',
        "This is the last useless page. It doesn't do anything useful at all.",
        self.files['site2_page1'], 'page 1', self.files['site2_page2'],
        'page 2', self.files['site2_home'])

    file_content[self.files['site2_signin']] = self.SIGNIN_CONTENT % (
        'Site Two signin', 'You cannot register online with this site.',
        self.files['site2_home'])

    for filename, content in file_content.iteritems():
      f = open(filename, 'w')
      try:
        f.write(content)
      finally:
        f.close()

  def setUp(self):
    self.cwd = os.getcwdu()
    self.temp_dir = tempfile.mkdtemp()
    os.chdir(self.temp_dir)

    self.files = {}

    self.CreateMockSiteOne()
    self.CreateMockSiteTwo()
    self.files['cookie'] = 'test.cookie'
    self.url1 = 'http://localhost:%s/%s' % (self.PORT1,
                                            self.files['site1_home'])
    self.url2 = 'http://localhost:%s/%s' % (self.PORT2,
                                            self.files['site2_home'])
    self.domain1 = 'localhost:%s' %self.PORT1
    self.files['url'] = 'urls.txt'
    url_file_handler = open(self.files['url'], 'w')
    try:
      url_file_handler.write('URLs to crawl:')
      url_file_handler.write(os.linesep)
      for url in (self.url1, self.url2):
        url_file_handler.write(url)
        url_file_handler.write(os.linesep)
    finally:
      url_file_handler.close()

    command_line = 'python -u -m SimpleHTTPServer %s' % self.PORT1
    args = command_line.split()
    self.server1 = subprocess.Popen(
        args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    self.server1.stdout.readline()  # Needed in order for the server to start up

    command_line = 'python -u -m SimpleHTTPServer %s' % self.PORT2
    args = command_line.split()
    self.server2 = subprocess.Popen(
        args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    self.server2.stdout.readline()  # Needed in order for the server to start up

  def tearDown(self):
    self.server1.terminate()
    self.server2.terminate()

    for filename in self.files.values():
      if os.path.isfile(filename):
        os.unlink(filename)
    os.chdir(self.cwd)
    os.rmdir(self.temp_dir)

  def testRetrieverDownloadsPage(self):
    """Verify the retriever can download a page."""
    r = webforms_aggregator.Retriever(self.url1, self.domain1,
                                      self.files['cookie'])
    self.assertTrue(r.Download(),
                msg='Retriever could not download "%s"' % self.url1)

  def testCrawlerFindsRegPageFromUrl(self):
    """Verify that the crawler is able to find a reg page from the given URL."""
    c = webforms_aggregator.Crawler(self.url1)
    self.assertTrue(
        c.Run(), msg='Crawler could not find the reg page of "%s"' % self.url1)

  def testCrawlerCannotFindNonExistentRegPageFromUrl(self):
    """Verify that the crawler won't find a non existent reg page
    from the given URL."""
    c = webforms_aggregator.Crawler(self.url2)
    self.assertFalse(
        c.Run(),
        msg='Crawler found a non existent reg page of "%s"' % self.url1)

  def testThreadedCrawlerFindsRegPageFromUrlsFile(self):
    """Verify the threaded crawler finds reg page from a file of URLs."""
    c = webforms_aggregator.ThreadedCrawler(self.files['url'])
    self.assertNotEqual(
        c.Run(), -1,
        msg='Threaded crawler could not find the reg page from the URLs file')


if __name__ == '__main__':
  suite = unittest.TestLoader().loadTestsFromTestCase(
      WebformsAggregatorTest)
  unittest.TextTestRunner(verbosity=2).run(suite)

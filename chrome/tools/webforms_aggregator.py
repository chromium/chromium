#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Downloads web pages with fillable forms after parsing through a set of links.

Used for collecting web pages with forms. Used as a standalone script.
This script assumes that it's run from within the same directory in which it's
checked into. If this script were to be run elsewhere then the path for
REGISTER_PAGE_DIR needs to be changed.

This script assumes that third party modules are installed:
httplib2, lxml, pycurl.

Usage: webforms_aggregator.py [options] [single url or file containing urls]

Options:
  -l LOG_LEVEL, --log_level LOG_LEVEL
    LOG_LEVEL: debug, info, warning or error [default: error]
  -h, --help  show this help message and exit
"""

import datetime
import errno
import logging
import optparse
import os
import re
# Needed in Linux so that PyCurl does not throw a segmentation fault.
import signal
import sys
import tempfile
import threading
import time
import urlparse

import httplib2
from lxml import html, etree
import pycurl

REGISTER_PAGE_DIR = os.path.join(os.pardir, 'test', 'data', 'autofill',
                                 'heuristics', 'input')
NOT_FOUND_REG_PAGE_SITES_FILENAME = 'notFoundRegPageSites.txt'

FORM_LOCATION_COMMENT = 'Form Location: %s'
HTML_FILE_PREFIX = 'grabber-'

MAX_REDIRECTIONS = 10

# Strings in a webpage that are indicative of a registration link.
LINK_CLUES = ['regist', 'user', 'sign', 'login', 'account']

MAX_SAME_DOMAIN_URLS_NO = 30
MAX_TOTAL_URLS_PER_DOMAIN = 300
MAX_OPEN_FILES_NO = 500

# URLs are selected for downloading with the following rules from the link
# lists, giving more weight to the links that contain a link clue.
CLUE_SECURE_LINKS_NO = MAX_SAME_DOMAIN_URLS_NO * 3/10
CLUE_GENERAL_LINKS_NO = MAX_SAME_DOMAIN_URLS_NO * 3/10
SECURE_LINKS_NO = MAX_SAME_DOMAIN_URLS_NO * 2/10
GENERAL_LINKS_NO = MAX_SAME_DOMAIN_URLS_NO * 2/10

MAX_ALLOWED_THREADS = MAX_OPEN_FILES_NO / MAX_SAME_DOMAIN_URLS_NO + 1


class Retriever(object):
  """Download, parse, and check if the web page contains a registration form.

  The objects of this class has a one to one relation with the web pages. For
  each page that is downloaded and parsed an object of this class is created.
  Each Retriever object creates a curl object. This object is added to the curl
  multi object of the crawler object so that the corresponding pages gets
  downloaded.
  """
  logger = logging.getLogger(__name__)

  def __init__(self, url, domain, cookie_file):
    """Initializes a Retriever object.

    Args:
      url: url to download page from.
      domain: only links with this domain will be retrieved.
      cookie_file: the name of a cookie file, needed for pages that use session
          cookies to change their contents.
    """
    self._url = url
    self._domain = domain
    self._html_content = ''

    # Http links without clues from LINK_CLUES.
    self._general_links = []
    # Http links that contain a clue from LINK_CLUES.
    self._clues_general_links = []
    # Https links that do not contain any clues from LINK_CLUES.
    self._secure_links = []
    # Https links that contain a clue from LINK_CLUES.
    self._clues_secure_links = []
    self._cookie_file = cookie_file
    self._curl_object = None

  def __del__(self):
    """Cleans up before this object is destroyed.

    The function closes the corresponding curl object that does the downloading.
    """
    if self._curl_object:
      self._curl_object.close()

  def _AddLink(self, link):
    """Adds url |link|, if not already present, to the appropriate list.

    The link only gets added to the single list that is appopriate for it:
    _secure_links, _general_links, _clues_secure_links or _clues_general_links.

    Args:
      link: the url that is inserted to the appropriate links list.
    """
    # Handles sites with unicode URLs.
    if isinstance(link, unicode):
      # Encode in 'utf-8' to avoid the UnicodeEncodeError exception.
      link = httplib2.iri2uri(link).encode('utf-8')
    link_parsed = urlparse.urlparse(link)
    link_lists = [self._clues_secure_links, self._secure_links,
                  self._clues_general_links, self._general_links]
    # Checks that the registration page is within the domain.
    if (self._domain in link_parsed[1] and
        all(link not in x for x in link_lists)):
      for clue in LINK_CLUES:
        if clue in link.lower():
          if link_parsed[0].startswith('https'):
            self._clues_secure_links.append(link)
            return
          else:
            self._clues_general_links.append(link)
            return
      if link_parsed[0].startswith('https'):  # No clues found in the link.
        self._secure_links.append(link)
      else:
        self._general_links.append(link)

  def ParseAndGetLinks(self):
    """Parses downloaded page and gets url link for non registration page.

    Checks if current page contains a registration page and if not it gets
    the url links. If it is a registration page, it saves it in a file as
    'grabber-' + domain + '.html' after it has added the FORM_LOCATION_COMMENT
    and it returns True. Otherwise it returns False.

    Returns:
      True if current page contains a registration form, and False otherwise.

    Raises:
      IOError: When can't write to the file.
    """
    if not self._domain:
      self.logger.error('Error: self._domain was not set')
      sys.exit(1)
    match_list = re.findall(r'(?P<quote>[\'\"])(?P<link>(?:https?:)?//.*?)\1',
                             self._html_content)
    for group_list in match_list:
      link = group_list[1]
      if link.startswith('//'):
        link = urlparse.urljoin(self._url, link)
      self._AddLink(link)
    try:
      tree = html.fromstring(self._html_content, parser=html.HTMLParser())
    except etree.LxmlError:
      self.logger.info('\t\tSkipping: not valid HTML code in this page <<< %s',
                       self._url)
      return False
    try:
      body = tree.iter('body').next()
    except StopIteration:
      self.logger.info('\t\tSkipping: no "BODY" tag in this page <<< %s',
                       self._url)
      return False

    # Get a list of all input elements with attribute type='password'
    password_elements = list(body.iterfind('.//input[@type="password"]'))
    # Check for multiple password elements to distinguish between a login form
    # and a registration form (Password field and Confirm Password field).
    if password_elements and len(password_elements) >= 2:
      form_elements = []
      for password_elem in password_elements:
        form_elem = password_elem.xpath('ancestor::form[1]')
        if not form_elem:
          continue
        if not form_elem[0] in form_elements:
          form_elements.append(form_elem[0])
        else:
          # Confirms that the page contains a registration form if two passwords
          # are contained in the same form for form_elem[0].
          if not os.path.isdir(REGISTER_PAGE_DIR):
            os.makedirs(REGISTER_PAGE_DIR)
          # Locate the HTML tag and insert the form location comment after it.
          html_tag = tree.iter('html').next()
          comment = etree.Comment(FORM_LOCATION_COMMENT % self._url)
          html_tag.insert(0, comment)
          # Create a new file and save the HTML registration page code.
          f = open('%s/%s%s.html' % (REGISTER_PAGE_DIR, HTML_FILE_PREFIX,
                                     self._domain), 'w')
          try:
            f.write(html.tostring(tree, pretty_print=True))
          except IOError as e:
            self.logger.error('Error: %s', e)
            raise
          finally:
            f.close()
          return True  # Registration page found.
    # Indicates page is not a registration page and links must be parsed.
    link_elements = list(body.iter('a'))
    for link_elem in link_elements:
      link = link_elem.get('href')
      if not link or '#' == link[0]:
        continue
      link = urlparse.urljoin(self._url, link)
      link_parsed = urlparse.urlparse(link)
      if not link_parsed[0].startswith('http'):
        continue
      self._AddLink(link)
    return False  # Registration page not found.

  def InitRequestHead(self):
    """Initializes curl object for a HEAD request.

    A HEAD request is initiated so that we can check from the headers if this is
    a valid HTML file. If it is not a valid HTML file, then we do not initiate a
    GET request, saving any unnecessary downloadings.
    """
    self._curl_object = pycurl.Curl()
    self._curl_object.setopt(pycurl.URL, self._url)
    # The following line fixes the GnuTLS package error that pycurl depends
    # on for getting https pages.
    self._curl_object.setopt(pycurl.SSLVERSION, pycurl.SSLVERSION_SSLv3)
    self._curl_object.setopt(pycurl.FOLLOWLOCATION, True)
    self._curl_object.setopt(pycurl.NOBODY, True)
    self._curl_object.setopt(pycurl.SSL_VERIFYPEER, False);
    self._curl_object.setopt(pycurl.MAXREDIRS, MAX_REDIRECTIONS)
    self._curl_object.setopt(pycurl.FAILONERROR, False)
    self._curl_object.setopt(pycurl.COOKIEFILE, self._cookie_file)
    self._curl_object.setopt(pycurl.COOKIEJAR, self._cookie_file)
    self._curl_object.setopt(pycurl.CONNECTTIMEOUT, 30)
    self._curl_object.setopt(pycurl.TIMEOUT, 300)
    self._curl_object.setopt(pycurl.NOSIGNAL, 1)

  def InitRequestGet(self):
    """Initializes curl object for a GET request.

    This is called only for valid HTML files. The Pycurl makes a GET request.
    The page begins to download, but since not all the data of the pages comes
    at once. When some of the data on the page is downloaded Pycurl will put
    this data in the buffer. The data is appended to the end of the page until
    everything is downloaded.
    """
    self._curl_object.setopt(pycurl.NOBODY, False)
    self._curl_object.setopt(
        pycurl.WRITEFUNCTION, lambda buff: setattr(
            self, '_html_content', self._html_content + buff))

  def Download(self):
    """Downloads the self._url page.

    It first does a HEAD request and then it proceeds to a GET request.
    It uses a curl object for a single download. This function is called only
    once for the initial url of a site when we still don't have more urls from a
    domain.

    Returns:
      True, if the downloaded page is valid HTML code, or False otherwise.
    """
    self.InitRequestHead()
    try:
      self._curl_object.perform()
    except pycurl.error as e:
      self.logger.error('Error: %s, url: %s', e, self._url)
      return False
    self._url = urlparse.urljoin(
        self._url, self._curl_object.getinfo(pycurl.EFFECTIVE_URL))
    content_type = self._curl_object.getinfo(pycurl.CONTENT_TYPE)
    if content_type and ('text/html' in content_type.lower()):
      self.InitRequestGet()
      try:
        self._curl_object.perform()
      except pycurl.error as e:
        self.logger.error('Error: %s, url: %s', e, self._url)
        return False
      return True
    else:
      self.logger.info('\tSkipping: Not an HTML page <<< %s', self._url)
      return False

  def Run(self):
    """Called only once for the initial url when we do not have more urls.

    Downloads the originally-specified site url, parses it and gets the links.

    Returns:
      True, if a registration page is found, and False otherwise.
    """
    if self.Download():
      if not self._domain:
        url_parsed = urlparse.urlparse(self._url)
        self._domain = url_parsed[1]
        if self._domain.startswith('www'):
          self._domain = '.'.join(self._domain.split('.')[1:])
      if self.ParseAndGetLinks():
        return True
    return False


class Crawler(object):
  """Crawls a site until a registration page is found or max level is reached.

  Creates, uses and destroys Retriever objects. Creates a cookie temp file
  needed for session cookies. It keeps track of 'visited links' and
  'links to visit' of the site. To do this it uses the links discovered from
  each Retriever object. Use Run() to crawl the site.
  """
  try:
    signal.signal(signal.SIGPIPE, signal.SIG_IGN)
  except ImportError:
    pass
  logger = logging.getLogger(__name__)

  def __init__(self, url, logging_level=None):
    """Init crawler URL, links lists, logger, and creates a cookie temp file.

    The cookie temp file is needed for session cookies.

    Args:
      url: the initial "seed" url of the site.
      logging_level: the desired verbosity level, default is None.
    """
    if logging_level:
      self.logger.setLevel(logging_level)

    self.url_error = False
    url_parsed = urlparse.urlparse(url)
    if not url_parsed[0].startswith('http'):
      self.logger.error(
          'Error: "%s" does not begin with http:// or https://', url)
      self.url_error = True
      return
    # Example: if url is 'http://www.example.com?name=john' then value [1] or
    # network location is 'www.example.com'.
    if not url_parsed[1]:
      self.logger.error('Error: "%s" is not a valid url', url)
      self.url_error = True
      return
    self._url = url
    self._domain = ''
    # Http links that contain a clue from LINK_CLUES.
    self._clues_general_links = []
    # Http links that do not contain any clue from LINK_CLUES.
    self._general_links = []
    # Https links that contain a clue from LINK_CLUES.
    self._clues_secure_links = []
    # Https links that do not contain any clue from LINK_CLUES.
    self._secure_links = []
    # All links downloaded and parsed so far.
    self._links_visited = []
    self._retrievers_list = []
    self._cookie_file = tempfile.NamedTemporaryFile(
        suffix='.cookie', delete=False)
    self._cookie_file.close()
    self._cookie_file = self._cookie_file.name  # Keep only the filename.

  def __del__(self):
    """Deletes cookie file when Crawler instances are destroyed."""
    if hasattr(self, '_cookie_file'):
      self.logger.info('Deleting cookie file %s ...', self._cookie_file)
      os.unlink(self._cookie_file)

  def _MultiPerform(self, curl_multi_object):
    """Performs concurrent downloads using a CurlMulti object.

    Args:
      curl_multi_object: a curl object that downloads multiple pages
          concurrently. The class of this object is |pycurl.CurlMulti|.
    """
    # Following code uses the example from section for the CurlMulti object
    # at http://pycurl.sourceforge.net/doc/curlmultiobject.html.
    while True:
      ret, no_handles = curl_multi_object.perform()
      if ret != pycurl.E_CALL_MULTI_PERFORM:
        break
    while no_handles:
      curl_multi_object.select(1.0)
      while True:
        ret, no_handles = curl_multi_object.perform()
        if ret != pycurl.E_CALL_MULTI_PERFORM:
          break

  def _GetLinksPages(self, curl_multi_object):
    """Downloads many pages concurrently using a CurlMulti Object.

    Creates many Retriever objects and adds them to a list. The constant
    MAX_SAME_DOMAIN_URLS_NO defines the number of pages that can be downloaded
    concurrently from the same domain using the pycurl multi object. It's
    currently set to 30 URLs. These URLs are taken from the links lists, which
    are from csl, gcl, sl, and gl. The rules define how many URLs are taken from
    each list during each iteration.

    Example of the rules:
      3/10 from csl results in 9 URLs
      3/10 from cgl results in 9 URLs
      2/10 from sl results in 6 URLs
      2/10 from gl results in 6 URLs

    Adding up the above URLs gives 30 URLs that can be downloaded concurrently.
    If these lists have fewer items than the defined rules, such as if a site
    does not contain any secure links, then csl and sl lists will be of 0 length
    and only 15 pages would be downloaded concurrently from the same domain.

    Since 30 URLs can be handled concurrently, the number of links taken from
    other lists can be increased. This means that we can take 24 links from the
    cgl list so that 24 from gfl + 6 from gl = 30 URLs. If the cgl list has less
    than 24 links, e.g. there are only 21 links, then only 9 links may be taken
    from gl so ) + 21 + 0 + 9 = 30.

    Args:
      curl_multi_object: Each Retriever object has a curl object which is
          added to the CurlMulti Object.
    """
    self._retrievers_list = []

    csl_no = min(CLUE_SECURE_LINKS_NO, len(self._clues_secure_links))
    cgl_no = min(CLUE_GENERAL_LINKS_NO, len(self._clues_general_links))
    sl_no = min(SECURE_LINKS_NO, len(self._secure_links))
    gl_no = min(GENERAL_LINKS_NO, len(self._general_links))

    # If some links within the list have fewer items than needed, the missing
    # links will be taken by the following priority: csl, cgl, sl, gl.
    # c: clues, s: secure, g: general, l: list.
    spare_links = MAX_SAME_DOMAIN_URLS_NO - (csl_no + sl_no + cgl_no + gl_no)
    if spare_links > 0:
      csl_no = min(csl_no + spare_links, len(self._clues_secure_links))
      spare_links = MAX_SAME_DOMAIN_URLS_NO - (csl_no + sl_no + cgl_no + gl_no)
    if spare_links > 0:
      cgl_no = min(cgl_no + spare_links, len(self._clues_general_links))
      spare_links = MAX_SAME_DOMAIN_URLS_NO - (csl_no + sl_no + cgl_no + gl_no)
    if spare_links > 0:
      sl_no = min(sl_no + spare_links, len(self._secure_links))
      spare_links = MAX_SAME_DOMAIN_URLS_NO - (csl_no + sl_no + cgl_no + gl_no)
    if spare_links > 0:
      gl_no = min(gl_no + spare_links, len(self._general_links))

    for no_of_links, links in [
        (csl_no, self._clues_secure_links),
        (sl_no, self._secure_links),
        (cgl_no, self._clues_general_links),
        (gl_no, self._general_links)]:
      for i in xrange(no_of_links):
        if not links:
          break
        url = links.pop(0)
        self._links_visited.append(url)
        r = Retriever(url, self._domain, self._cookie_file)
        r.InitRequestHead()
        curl_multi_object.add_handle(r._curl_object)
        self._retrievers_list.append(r)

    if self._retrievers_list:
      try:
        self._MultiPerform(curl_multi_object)
      except pycurl.error as e:
        self.logger.error('Error: %s, url: %s', e, self._url)
      finally:
        for r in self._retrievers_list:
          curl_multi_object.remove_handle(r._curl_object)
      # |_retrievers_list[:]| is a copy of |_retrievers_list| to avoid removing
      # items from the iterated list.
      for r in self._retrievers_list[:]:
        r._url = urlparse.urljoin(r._url, r._curl_object.getinfo(
            pycurl.EFFECTIVE_URL))
        content_type = r._curl_object.getinfo(pycurl.CONTENT_TYPE)
        if content_type and ('text/html' in content_type.lower()):
          r.InitRequestGet()
          curl_multi_object.add_handle(r._curl_object)
        else:
          self._retrievers_list.remove(r)
          self.logger.info('\tSkipping: Not an HTML page <<< %s', r._url)
      if self._retrievers_list:
        try:
          self._MultiPerform(curl_multi_object)
        except pycurl.error as e:
          self.logger.error('Error: %s, url: %s', e, self._url)
        finally:
          for r in self._retrievers_list:
            curl_multi_object.remove_handle(r._curl_object)
            self.logger.info('Downloaded: %s', r._url)

  def _LogRegPageFound(self, retriever):
    """Display logging for registration page found.

    Args:
      retriever: The object that has retrieved the page.
    """
    self.logger.info('\t##############################################')
    self.logger.info('\t### %s ###', retriever._domain)
    self.logger.info('\t##############################################')
    self.logger.info('\t!!!!!!!!!  registration page FOUND !!!!!!!!!!!')
    self.logger.info('\t%s', retriever._url)
    self.logger.info('\t##############################################')

  def _GetNewLinks(self, retriever):
    """Appends new links discovered by each retriever to the appropriate lists.

    Links are copied to the links list of the crawler object, which holds all
    the links found from all retrievers that the crawler object created. The
    Crawler object exists as far as a specific site is examined and the
    Retriever object exists as far as a page of this site is examined.

    Args:
      retriever: a temporary object that downloads a specific page, parses the
          content and gets the page's href link.
    """
    for link in retriever._clues_secure_links:
      if (not link in self._clues_secure_links and
          not link in self._links_visited):
        self._clues_secure_links.append(link)
    for link in retriever._secure_links:
      if (not link in self._secure_links and
          not link in self._links_visited):
        self._secure_links.append(link)
    for link in retriever._clues_general_links:
      if (not link in self._clues_general_links and
          not link in self._links_visited):
        self._clues_general_links.append(link)
    for link in retriever._general_links:
      if (not link in self._general_links and
          not link in self._links_visited):
        self._general_links.append(link)

  def Run(self):
    """Runs the Crawler.

    Creates a Retriever object and calls its run method to get the first links,
    and then uses CurlMulti object and creates many Retriever objects to get
    the subsequent pages.

    The number of pages (=Retriever objs) created each time is restricted by
    MAX_SAME_DOMAIN_URLS_NO. After this number of Retriever objects download
    and parse their pages, we do the same again. The number of total pages
    visited is kept in urls_visited.
    If no registration page is found, the Crawler object will give up its try
    after MAX_TOTAL_URLS_PER_DOMAIN is reached.

    Returns:
      True is returned if registration page is found, or False otherwise.
    """
    reg_page_found = False
    if self.url_error:
      return False
    r = Retriever(self._url, self._domain, self._cookie_file)
    if r.Run():
      self._LogRegPageFound(r)
      reg_page_found = True
    else:
      self._url = r._url
      self._domain = r._domain
      self.logger.info('url to crawl: %s', self._url)
      self.logger.info('domain: %s', self._domain)
      self._links_visited.append(r._url)
      self._GetNewLinks(r)
      urls_visited = 1
      while True:
        if (not (self._clues_secure_links or self._secure_links or
                self._clues_general_links or self._general_links) or
            urls_visited >= MAX_TOTAL_URLS_PER_DOMAIN):
          break  # Registration page not found.
        m = pycurl.CurlMulti()
        self._GetLinksPages(m)
        urls_visited += len(self._retrievers_list)
        self.logger.info('\t<----- URLs visited for domain "%s": %d ----->',
                         self._domain, urls_visited)
        for r in self._retrievers_list:
          if r.ParseAndGetLinks():
            self._LogRegPageFound(r)
            reg_page_found = True
            break
          else:
            self.logger.info('parsed: %s', r._url)
            self._GetNewLinks(r)
        m.close()
        if reg_page_found:
          break
    while self._retrievers_list:
      r = self._retrievers_list.pop()
    return reg_page_found


class WorkerThread(threading.Thread):
  """Creates a new thread of execution."""
  def __init__(self, url):
    """Creates _url and page_found attri to populate urls_with_no_reg_page file.

    Used after thread's termination for the creation of a file with a list of
    the urls for which a registration page wasn't found.

    Args:
      url: will be used as an argument to create a Crawler object later.
    """
    threading.Thread.__init__(self)
    self._url = url
    self.page_found = False

  def run(self):
    """Execution of thread creates a Crawler object and runs it.

    Caution: this function name should not be changed to 'Run' or any other
    names because it is overriding the 'run' method of the 'threading.Thread'
    class. Otherwise it will never be called.
    """
    self.page_found = Crawler(self._url).Run()


class ThreadedCrawler(object):
  """Calls the Run function of WorkerThread which creates & runs a Crawler obj.

  The crawler object runs concurrently, examining one site each.
  """
  logger = logging.getLogger(__name__)

  def __init__(self, urls_file, logging_level=None):
    """Creates threaded Crawler objects.

    Args:
      urls_file: a text file containing a URL in each line.
      logging_level: verbosity level, default is None.

    Raises:
      IOError: If cannot find URLs from the list.
    """
    if logging_level:
      self.logger.setLevel(logging_level)

    self._urls_list = []
    f = open(urls_file)
    try:
      for url in f.readlines():
        url = url.strip()
        if not urlparse.urlparse(url)[0].startswith('http'):
          self.logger.info(
              '%s: skipping this (does not begin with "http://")', url)
          continue
        self._urls_list.append(url)
    except IOError as e:
      self.logger.error('Error: %s', e)
      raise
    finally:
      f.close()
    if not self._urls_list:
      error_msg = 'No URLs were found.'
      self.logger.error('ERROR: %s', error_msg)
      raise IOError(error_msg)

  def Run(self):
    """Runs Crawler objects using python threads.

    Number of concurrent threads is restricted to MAX_ALLOWED_THREADS.

    Returns:
      The number of registration pages found. -1 if no URLs are given.

    Raises:
      OSError: When creating the same directory that already exists.
    """
    if self._urls_list:
      allThreads = []
      # originalNumThreads is the number of threads just before the
      # ThreadedCrawler starts creating new threads. As a standalone script it
      # will be 1.
      originalNumThreads = threading.active_count()
      for url in self._urls_list:
        self.logger.info('URL fed to a crawler thread: %s', url)
        t = WorkerThread(url)
        t.start()
        allThreads.append(t)
        while threading.active_count() >= (
            MAX_ALLOWED_THREADS + originalNumThreads):
          time.sleep(.4)
      while threading.active_count() > originalNumThreads:
        time.sleep(.4)
      self.logger.info('----------------')
      self.logger.info('--- FINISHED ---')
      self.logger.info('----------------')
      urls_no = 0
      urls_not_found_no = 0
      not_file_name = os.path.join(
          REGISTER_PAGE_DIR, NOT_FOUND_REG_PAGE_SITES_FILENAME)
      not_file_dir = os.path.dirname(not_file_name)
      try:
        os.makedirs(not_file_dir)
      except OSError as e:
        if e.errno != errno.EEXIST:
          raise
      fnot = open(not_file_name, 'wb')
      try:
        for t in sorted(allThreads, key=lambda t: t._url):
          urls_no += 1
          if not t.page_found:
            urls_not_found_no += 1
            fnot.write('%s' % t._url)
            fnot.write(os.linesep)
      except IOError as e:
        self.logger.error('Error: %s', e)
      finally:
        fnot.close()
      self.logger.info('Total number of URLs given: %d\n', urls_no)
      self.logger.info(
          'Registration pages found: %d\n', (urls_no - urls_not_found_no))
      self.logger.info(
          'URLs that did not return a registration page: %d\n',
          urls_not_found_no)
      return urls_no - urls_not_found_no
    else:
      self.logger.error('Error: no URLs were found.')
      return -1


def main():
  usage = 'usage: %prog [options] single_url_or_urls_filename'
  parser = optparse.OptionParser(usage)
  parser.add_option(
      '-l', '--log_level', metavar='LOG_LEVEL', default='error',
      help='LOG_LEVEL: debug, info, warning or error [default: %default]')

  (options, args) = parser.parse_args()
  options.log_level = options.log_level.upper()
  if options.log_level not in ['DEBUG', 'INFO', 'WARNING', 'ERROR']:
    print 'Wrong log_level argument.'
    parser.print_help()
    return 1
  options.log_level = getattr(logging, options.log_level)

  if len(args) != 1:
    parser.error('Wrong number of arguments.')

  logger = logging.getLogger(__name__)
  if options.log_level:
    console = logging.StreamHandler()
    logger.addHandler(console)
    logger.setLevel(options.log_level)

  arg_is_a_file = os.path.isfile(args[0])
  if arg_is_a_file:
    CrawlerClass = ThreadedCrawler
  else:
    CrawlerClass = Crawler
  t0 = datetime.datetime.now()
  c = CrawlerClass(args[0], options.log_level)
  c.Run()
  if not arg_is_a_file and c.url_error:
    logger.error(
        'ERROR: "%s" is neither a valid filename nor a valid URL' % args[0])
  t1 = datetime.datetime.now()
  delta_t = t1 - t0
  logger.info('Started at: %s\n', t0)
  logger.info('Ended at: %s\n', t1)
  logger.info('Total execution time: %s\n', delta_t)
  return 0


if __name__ == "__main__":
  sys.exit(main())

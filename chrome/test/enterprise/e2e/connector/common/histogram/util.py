# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import socket
from typing import Dict, List, Optional, Set

from absl import logging
from attr import asdict
from attr import define
from attr import Factory
from selenium import webdriver


@define
class HistogramDataBucket:
  """Maps to the json:"buckets" values inside HistogramData.

  This class allows for the parsing of the "buckets" json response when using
  histogram collection in Chrome. For more information on histogram collection,
  see http://shortn/_Oar7S8pRLF.

  Attributes:
    count: The "count" value in the nested "buckets" json object of the json
      response. Defaults to None.
    high: The "high" value in the nested "buckets" json object of the json
      response. Defaults to None.
    low: The "low" value in the nested "buckets" json object of the json
      response. Defaults to None.
  """
  count: int = None
  high: int = None
  low: int = None

  def __bool__(self):
    return bool(self.count or self.high or self.low)


@define
class HistogramDataParams:
  """Maps to the json:"params" inside HistogramData.

  This class allows for the parsing of the "params" within the json response
  provided when using histogram collection in Chrome. For more information on
  histogram collection, see http://shortn/_Oar7S8pRLF.

  Attributes:
    bucket_count: The "bucket_count" value in the nested "params" json of the
      json response. Defaults to None.
    max_value: The "max" value in the nested "params" json of the json response.
      Defaults to None.
    min_value: The "min" value in the nested "params" json of the json response.
      Defaults to None.
    data_type: The "type" value in the nested "params" json of the json
      response. Defaults to None.
  """
  bucket_count: float = None
  max_value: float = None
  min_value: float = None
  data_type: str = None

  def __bool__(self):
    return bool(self.bucket_count or self.max_value or self.min_value or
                self.data_type)


@define
class HistogramData:
  """Maps to the json string schema from histogram.

  This class allows for the parsing of the json response provided when using
  histogram collection in Chrome. For more information on histogram collection,
  see http://shortn/_Oar7S8pRLF. All values default to None in the case of a
  partial json response.

  Attributes:
    buckets: The "buckets" value of the json response. Defaults to None.
    count: The "count" value of the json response. Defaults to None.
    flags: The "flags" value of the json response. Defaults to None.
    name: The "name" value of the json response. Defaults to None.
    params: The "params" value of the json response. Defaults to None.
    pid: The "pid" value of the json response. Defaults to None.
    sum_value: The "sum" value of the json response. Defaults to None.
  """
  buckets: List[HistogramDataBucket] = Factory(list)
  count: float = 0.0
  flags: float = None
  name: str = None
  params: HistogramDataParams = None
  pid: float = None
  sum_value: float = None
  values: Set[int] = Factory(set)

  def isValuePresent(self, value: int) -> bool:
    return value in self.values


def parse(response: str) -> HistogramData:
  """Parses a Chrome histogram response json string into a HistogramData object.

  Args:
    response: A string representation for the json response from a call to
      statsCollectionController.getBrowserHistogram.

  Returns:
    A HistogramData object represented the json response.

  Raises:
    BaseException: If the json response isn't able to be parsed into a
     HistogramData object.
  """
  d = json.loads(response)
  buckets_list = d["buckets"] if "buckets" in d else []
  buckets = [
      HistogramDataBucket(b["count"], b["high"], b["low"]) for b in buckets_list
  ] if buckets_list else None
  count = d["count"] if "count" in d else None
  flags = d["flags"] if "flags" in d else None
  name = d["name"] if "name" in d else None
  p = d["params"] if "params" in d else None
  params = HistogramDataParams(
      p["bucket_count"] if "bucket_count" in p else 0.0,
      p["max"] if "max" in p else -1, p["min"] if "min" in p else -1,
      p["type"] if "type" in p else None) if p else None
  pid = d["pid"] if "pid" in d else None
  s = d["sum"] if "sum" in d else None
  return HistogramData(buckets, count, flags, name, params, pid, s)


def add(val1: Optional[float], val2: Optional[float]) -> Optional[float]:
  if not val1 or not val2:
    return val2 if not val1 else val1
  return val1 + val2


def to_int(val: Optional[float]) -> int:
  """Returns a rounded up integer from a byte string."""
  if not val:
    return 0
  return int(float(str(val)))


def merge_histograms(hd1: Optional[HistogramData],
                     hd2: Optional[HistogramData]) -> Optional[HistogramData]:
  """Merges two histograms by summing corresponding values.

  Args:
    hd1: A HistogramData object to be merged.
    hd2: A HistogramData object to be merged.

  Returns:
    A HistogramData object in which all corresponding values are added.
  """
  if not hd1 or not hd2:
    return hd2 if not hd1 else hd1

  logging.info(f'hd1={hd1}, hd2={hd2}')
  hd1.count = add(hd1.count, hd2.count)
  hd1.sum_value = add(hd1.sum_value, hd2.sum_value)
  # Add both sum_value to values to allow query for present query
  # sum_value is byte format, and it is like b'9.0'
  try:
    hd1.values.add(to_int(hd1.sum_value))
    hd1.values.add(to_int(hd2.sum_value))
  except e:
    logging.warning(
        f'fail to add {hd1.sum_value}, {hd2.sum_value}. err: {str(e)}')

  if not hd2.buckets:
    return hd1

  for b2 in hd2.buckets:
    found = False
    for i, b1 in enumerate(hd1.buckets):
      if b2.low == b1.low and b2.high == b1.high:
        hd1.buckets[i].count += b2.count
        found = True

    if not found:
      hd1.buckets.append(b2)

  return hd1


def histogram(cd: webdriver.chrome.webdriver.WebDriver,
              name: str,
              timeout: int = 30) -> Optional[HistogramData]:
  """Queries a particular histogram from Chrome and formats the response.

  The ChromeDriver instance provided is used to execute javascript in browser,
  generating a histogram as a json response. The response is then parsed
  and processed, ultimately returned as a HistogramData object. For more
  information on histogram collection, see http://shortn/_Oar7S8pRLF.

  Args:
    cd: A selenium.webdriver.chrome.webdriver.WebDriver instance.
    name: The name of the histogram to search for.
    timeout: The timeout for the script execution in seconds. Defaults to 30.

  Returns:
    A HistogramData object parsed from the json response.

  Raises:
    BaseException: When either histogram request returns an empty response.
  """
  resp_str = execute_script(
      cd, "return statsCollectionController.getBrowserHistogram('%s');" % name,
      timeout)
  if not resp_str:
    raise BaseException("Could not get histogram with %s" % name)

  hd1 = parse(resp_str)

  # Fetch renderer histograms as well
  resp_str = execute_script(
      cd, "return statsCollectionController.getHistogram('%s');" % name)
  if not resp_str:
    raise ValueError("Could not get second histogram with %s" % name)
  hd2 = parse(resp_str)
  return merge_histograms(hd1, hd2)


def execute_script(cd: webdriver.chrome.webdriver.WebDriver,
                   script: str,
                   timeout: int = 30) -> str:
  """Executes javascript in browser through ChromeDriver.

  Args:
    cd: A selenium.webdriver.chrome.webdriver.WebDriver instance.
    script: The script to run in the form of a string.
    timeout: The timeout to update the socket to. Defaults to 30 seconds to
      ensure adequate time for the javascript response.

  Returns:
    A str of the response data from executing the script.
  """
  default_timeout = socket.getdefaulttimeout()
  socket.setdefaulttimeout(timeout)
  logging.log(logging.DEBUG, "Set socket timeout to %s seconds", timeout)

  script_result = cd.execute_script(script)
  logging.log(logging.DEBUG, "Executed Javascript in browser: %s", script)

  socket.setdefaulttimeout(default_timeout)
  logging.log(logging.DEBUG, "Set socket timeout to default: %s",
              default_timeout)
  return script_result


def poll_histogram(cd: webdriver.chrome.webdriver.WebDriver,
                   names: List[str],
                   timeout: int = 30) -> Optional[Dict[str, HistogramData]]:
  """Polls the histograms and returns the list of non-empty histogram.

  Args:
    cd: A selenium.webdriver.chrome.webdriver.WebDriver instance.
    names: The names of the histograms to fetch.
    timeout: The timeout for each histogram fetch request in seconds. Defaults
      to 30.

  Returns:
    A dict of HistogramData object of the matched histogram.
  """
  hgs = None
  for name in names:
    hg = histogram(cd, name, timeout)
    if not hgs:
      hgs = {}
    hgs[name] = asdict(hg)

  return hgs
